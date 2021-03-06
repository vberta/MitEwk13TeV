//================================================================================================
//
// Compute W->enu acceptance at full selection level
//
//  * outputs results summary text file
//
//  [!!!] propagation of efficiency scale factor uncertainties need to be checked
//________________________________________________________________________________________________

#if !defined(__CINT__) || defined(__MAKECINT__)
#include <TROOT.h>                  // access to gROOT, entry point to ROOT system
#include <TSystem.h>                // interface to OS
#include <TFile.h>                  // file handle class
#include <TTree.h>                  // class to access ntuples
#include <TClonesArray.h>           // ROOT array class
#include <TBenchmark.h>             // class to track macro running statistics
#include <TH1D.h>                   // histogram class
#include <TMath.h>                  // ROOT math library
#include <vector>                   // STL vector class
#include <iostream>                 // standard I/O
#include <iomanip>                  // functions to format standard I/O
#include <fstream>                  // functions for file I/O
#include <string>                   // C++ string class
#include <sstream>                  // class for parsing strings
#include "TLorentzVector.h"
#include <TRandom3.h>
#include "TGraph.h"
#include "TF1.h"

// define structures to read in ntuple
#include "BaconAna/DataFormats/interface/BaconAnaDefs.hh"
#include "BaconAna/DataFormats/interface/TEventInfo.hh"
#include "BaconAna/DataFormats/interface/TGenEventInfo.hh"
#include "BaconAna/DataFormats/interface/TElectron.hh"
#include "BaconAna/DataFormats/interface/TVertex.hh"
#include "BaconAna/Utils/interface/TTrigger.hh"

#include "../Utils/LeptonIDCuts.hh" // helper functions for lepton ID selection
#include "../Utils/MyTools.hh"
#include "../Utils/LeptonCorr.hh"         // Scale and resolution corrections
#include "../EleScale/EnergyScaleCorrection_class.hh" //EGMSmear

// helper class to handle efficiency tables
#include "CEffUser1D.hh"
#include "CEffUser2D.hh"
#endif


//=== MAIN MACRO ================================================================================================= 

void computeAccSelWe_Sys(const TString conf,       // input file
                     const TString outputDir,  // output directory
		     const Int_t   charge,      // 0 = inclusive, +1 = W+, -1 = W-
		     const Int_t   doPU,
		     const Int_t   doScaleCorr,
		     const Int_t   sigma,
		     const TString sysFile
) {
  gBenchmark->Start("computeAccSelWe");

  //--------------------------------------------------------------------------------------------------------------
  // Settings 
  //============================================================================================================== 

  const Double_t PT_CUT     = 25;
  const Double_t ETA_CUT    = 2.5;
  const Double_t ELE_MASS = 0.000511;

  const Double_t ETA_BARREL = 1.4442;
  const Double_t ETA_ENDCAP = 1.566;

  const Double_t VETO_PT   = 10;
  const Double_t VETO_ETA  = 2.5;

  const Double_t ECAL_GAP_LOW  = 1.4442;
  const Double_t ECAL_GAP_HIGH = 1.566;

  const Int_t BOSON_ID  = 24;
  const Int_t LEPTON_ID = 11;
 
  // efficiency files
  TString dataHLTEffName(   "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleHLTEff/MG/eff.root");
  TString zeeHLTEffName(    "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleHLTEff/CT/eff.root");
  TString dataGsfSelEffName("/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleGsfSelEff/MG/eff.root");
  TString zeeGsfSelEffName( "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleGsfSelEff/CT/eff.root");
  if(charge==1) {
    dataHLTEffName    = "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleHLTEff/MGpositive/eff.root";
    zeeHLTEffName     = "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleHLTEff/CTpositive/eff.root"; 
    dataGsfSelEffName = "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleGsfSelEff/MGpositive_FineBin/eff.root";
    zeeGsfSelEffName  = "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleGsfSelEff/CTpositive/eff.root"; 
  }
  if(charge==-1) {
    dataHLTEffName    = "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleHLTEff/MGnegative/eff.root";
    zeeHLTEffName     = "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleHLTEff/CTnegative/eff.root";
    dataGsfSelEffName = "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleGsfSelEff/MGnegative_FineBin/eff.root";
    zeeGsfSelEffName  = "/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleGsfSelEff/CTnegative/eff.root";
  }

  const TString corrFiles = "../EleScale/76X_16DecRereco_2015_Etunc";

  EnergyScaleCorrection_class eleCorr( corrFiles.Data()); eleCorr.doScale= true; eleCorr.doSmearings =true;

  // load pileup reweighting file
  TFile *f_rw = TFile::Open("../Tools/puWeights_76x.root", "read");
  TH1D *h_rw = (TH1D*) f_rw->Get("puWeights");

  TFile *f_r9 = TFile::Open("../EleScale/transformation.root","read");
  TGraph* gR9EB = (TGraph*) f_r9->Get("transformR90");
  TGraph* gR9EE = (TGraph*) f_r9->Get("transformR91");

  TFile *f_hlt_data;
  TFile *f_hlt_mc;

  if(charge==1){
    f_hlt_data = TFile::Open("/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleHLTEff/Nominal/EleTriggerTF1_Data_Positive.root");
    f_hlt_mc   = TFile::Open("/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleHLTEff/Nominal/EleTriggerTF1_MC_Positive.root");
  }
  if(charge==-1){
    f_hlt_data = TFile::Open("/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleHLTEff/Nominal/EleTriggerTF1_Data_Negative.root");
    f_hlt_mc   = TFile::Open("/afs/cern.ch/work/x/xniu/public/WZXSection/wz-efficiency/EleHLTEff/Nominal/EleTriggerTF1_MC_Negative.root");
  }

  TFile *f_sys = TFile::Open(sysFile);
  TH2D  *h_sys = (TH2D*) f_sys->Get("h");
 
  //--------------------------------------------------------------------------------------------------------------
  // Main analysis code 
  //==============================================================================================================  

  vector<TString> fnamev;  // file name per input file
  vector<TString> labelv;  // TLegend label per input file
  vector<Int_t>   colorv;  // plot color per input file
  vector<Int_t>   linev;   // plot line style per input file

  //
  // parse .conf file
  //
  ifstream ifs;
  ifs.open(conf.Data());
  assert(ifs.is_open());
  string line;
  while(getline(ifs,line)) {
    if(line[0]=='#') continue;
    
    string fname;
    Int_t color, linesty;
    stringstream ss(line);
    ss >> fname >> color >> linesty;
    string label = line.substr(line.find('@')+1);
    fnamev.push_back(fname);
    labelv.push_back(label);
    colorv.push_back(color);
    linev.push_back(linesty);
  }
  ifs.close();

  // Create output directory
  gSystem->mkdir(outputDir,kTRUE);
  
  //
  // Get efficiency
  //
  TFile *dataHLTEffFile = new TFile(dataHLTEffName);
  CEffUser2D dataHLTEff;
  TH2D *hHLTErr=0, *hHLTErrB=0, *hHLTErrE=0;
  if(dataHLTEffName) {
    dataHLTEff.loadEff((TH2D*)dataHLTEffFile->Get("hEffEtaPt"),
                       (TH2D*)dataHLTEffFile->Get("hErrlEtaPt"),
                       (TH2D*)dataHLTEffFile->Get("hErrhEtaPt"));
    
    TH2D* h =(TH2D*)dataHLTEffFile->Get("hEffEtaPt");
    hHLTErr  = new TH2D("hHLTErr", "",h->GetNbinsX(),h->GetXaxis()->GetXmin(),h->GetXaxis()->GetXmax(),
                                      h->GetNbinsY(),h->GetYaxis()->GetXmin(),h->GetYaxis()->GetXmax());
    hHLTErrB = new TH2D("hHLTErrB","",h->GetNbinsX(),h->GetXaxis()->GetXmin(),h->GetXaxis()->GetXmax(),
                                      h->GetNbinsY(),h->GetYaxis()->GetXmin(),h->GetYaxis()->GetXmax());
    hHLTErrE = new TH2D("hHLTErrE","",h->GetNbinsX(),h->GetXaxis()->GetXmin(),h->GetXaxis()->GetXmax(),
                                      h->GetNbinsY(),h->GetYaxis()->GetXmin(),h->GetYaxis()->GetXmax());
  }
  
  TFile *zeeHLTEffFile = new TFile(zeeHLTEffName);
  CEffUser2D zeeHLTEff;
  if(zeeHLTEffName) {
    zeeHLTEff.loadEff((TH2D*)zeeHLTEffFile->Get("hEffEtaPt"),
                      (TH2D*)zeeHLTEffFile->Get("hErrlEtaPt"),
                      (TH2D*)zeeHLTEffFile->Get("hErrhEtaPt"));
  }
  
  TFile *dataGsfSelEffFile = new TFile(dataGsfSelEffName);
  CEffUser2D dataGsfSelEff;
  TH2D *hGsfSelErr=0, *hGsfSelErrB=0, *hGsfSelErrE=0;
  if(dataGsfSelEffName) {
    dataGsfSelEff.loadEff((TH2D*)dataGsfSelEffFile->Get("hEffEtaPt"),
                       (TH2D*)dataGsfSelEffFile->Get("hErrlEtaPt"),
                       (TH2D*)dataGsfSelEffFile->Get("hErrhEtaPt"));
    
    TH2D* h =(TH2D*)dataGsfSelEffFile->Get("hEffEtaPt");
    hGsfSelErr  = new TH2D("hGsfSelErr", "",h->GetNbinsX(),h->GetXaxis()->GetXmin(),h->GetXaxis()->GetXmax(),
                                      h->GetNbinsY(),h->GetYaxis()->GetXmin(),h->GetYaxis()->GetXmax());
    hGsfSelErrB = new TH2D("hGsfSelErrB","",h->GetNbinsX(),h->GetXaxis()->GetXmin(),h->GetXaxis()->GetXmax(),
                                      h->GetNbinsY(),h->GetYaxis()->GetXmin(),h->GetYaxis()->GetXmax());
    hGsfSelErrE = new TH2D("hGsfSelErrE","",h->GetNbinsX(),h->GetXaxis()->GetXmin(),h->GetXaxis()->GetXmax(),
                                      h->GetNbinsY(),h->GetYaxis()->GetXmin(),h->GetYaxis()->GetXmax());
  }
  
  TFile *zeeGsfSelEffFile = new TFile(zeeGsfSelEffName);
  CEffUser2D zeeGsfSelEff;
  if(zeeGsfSelEffName) {
    zeeGsfSelEff.loadEff((TH2D*)zeeGsfSelEffFile->Get("hEffEtaPt"),
                      (TH2D*)zeeGsfSelEffFile->Get("hErrlEtaPt"),
                      (TH2D*)zeeGsfSelEffFile->Get("hErrhEtaPt"));
  }
  
  // Data structures to store info from TTrees
  baconhep::TEventInfo    *info = new baconhep::TEventInfo();
  baconhep::TGenEventInfo *gen  = new baconhep::TGenEventInfo();
  TClonesArray *electronArr = new TClonesArray("baconhep::TElectron");
  TClonesArray *genPartArr  = new TClonesArray("baconhep::TGenParticle");
  TClonesArray *vertexArr  = new TClonesArray("baconhep::TVertex");
  
  TFile *infile=0;
  TTree *eventTree=0;

  // Variables to store acceptances and uncertainties (per input file)
  vector<Double_t> nEvtsv, nSelv, nSelBv, nSelEv;
  vector<Double_t> accv, accBv, accEv;
  vector<Double_t> accErrv, accErrBv, accErrEv;
  vector<Double_t> nSelCorrv, nSelBCorrv, nSelECorrv;
  vector<Double_t> nSelCorrVarv, nSelBCorrVarv, nSelECorrVarv;
  vector<Double_t> accCorrv, accBCorrv, accECorrv;
  vector<Double_t> accErrCorrv, accErrBCorrv, accErrECorrv;

  const baconhep::TTrigger triggerMenu("../../BaconAna/DataFormats/data/HLT_50nsGRun");
 
  // loop through files
  //
  for(UInt_t ifile=0; ifile<fnamev.size(); ifile++) {  

    // Read input file and get the TTrees
    cout << "Processing " << fnamev[ifile] << " ..." << endl;
    infile = TFile::Open(fnamev[ifile]); 
    assert(infile);
  
    eventTree = (TTree*)infile->Get("Events"); assert(eventTree);  
    eventTree->SetBranchAddress("Info",             &info); TBranch *infoBr     = eventTree->GetBranch("Info");
    eventTree->SetBranchAddress("GenEvtInfo",        &gen); TBranch *genBr      = eventTree->GetBranch("GenEvtInfo");
    eventTree->SetBranchAddress("GenParticle",&genPartArr); TBranch *genPartBr  = eventTree->GetBranch("GenParticle");
    eventTree->SetBranchAddress("Electron",  &electronArr); TBranch *electronBr = eventTree->GetBranch("Electron");
    eventTree->SetBranchAddress("PV",   &vertexArr); TBranch *vertexBr = eventTree->GetBranch("PV");

    nEvtsv.push_back(0);
    nSelv.push_back(0);
    nSelBv.push_back(0);
    nSelEv.push_back(0);
    nSelCorrv.push_back(0);
    nSelBCorrv.push_back(0);
    nSelECorrv.push_back(0);
    nSelCorrVarv.push_back(0);
    nSelBCorrVarv.push_back(0);
    nSelECorrVarv.push_back(0);
    
    for(Int_t iy=0; iy<=hHLTErr->GetNbinsY(); iy++) {
      for(Int_t ix=0; ix<=hHLTErr->GetNbinsX(); ix++) {
        hHLTErr ->SetBinContent(ix,iy,0);
        hHLTErrB->SetBinContent(ix,iy,0);
        hHLTErrE->SetBinContent(ix,iy,0);
      }
    }
    for(Int_t iy=0; iy<=hGsfSelErr->GetNbinsY(); iy++) {
      for(Int_t ix=0; ix<=hGsfSelErr->GetNbinsX(); ix++) {
        hGsfSelErr ->SetBinContent(ix,iy,0);
        hGsfSelErrB->SetBinContent(ix,iy,0);
        hGsfSelErrE->SetBinContent(ix,iy,0);
      }
    }

    //
    // loop over events
    //
    for(UInt_t ientry=0; ientry<eventTree->GetEntries(); ientry++) {
//      if(ientry==10000) break;
      infoBr->GetEntry(ientry);
      genBr->GetEntry(ientry);      
      genPartArr->Clear(); genPartBr->GetEntry(ientry);
  
      if (charge==-1 && toolbox::flavor(genPartArr, -BOSON_ID)!=LEPTON_ID) continue;
      if (charge==1 && toolbox::flavor(genPartArr, BOSON_ID)!=-LEPTON_ID) continue;
      if (charge==0 && fabs(toolbox::flavor(genPartArr, BOSON_ID))!=LEPTON_ID) continue;
    
      vertexArr->Clear();
      vertexBr->GetEntry(ientry);
      double npv  = vertexArr->GetEntries();
      Double_t weight=gen->weight;
      if(doPU>0) weight*=h_rw->GetBinContent(h_rw->FindBin(info->nPUmean));

      nEvtsv[ifile]+=weight;
      
      // trigger requirement                
      if (!isEleTrigger(triggerMenu, info->triggerBits, kFALSE)) continue;
      
      // good vertex requirement
      if(!(info->hasGoodPV)) continue;
      
      electronArr->Clear();
      electronBr->GetEntry(ientry);
      Int_t nLooseLep=0;
      const baconhep::TElectron *goodEle=0;
      TLorentzVector vEle(0,0,0,0);
      TLorentzVector vElefinal(0,0,0,0);
      Bool_t passSel=kFALSE;
     
      for(Int_t i=0; i<electronArr->GetEntriesFast(); i++) {
        const baconhep::TElectron *ele = (baconhep::TElectron*)((*electronArr)[i]);
	vEle.SetPtEtaPhiM(ele->pt, ele->eta, ele->phi, ELE_MASS);
	
	//double ele_pt = gRandom->Gaus(ele->scEt*getEleScaleCorr(ele->scEta,0), getEleResCorr(ele->scEta,0));
        //double ele_pt = gRandom->Gaus(ele->scEt*getEleScaleCorr(ele->scEta,0), getEleResCorr(ele->scEta,0));

        // check ECAL gap
//        if(fabs(ele->scEta)>=ETA_BARREL && fabs(ele->scEta)<=ETA_ENDCAP) continue;
	if(fabs(vEle.Eta())>=ECAL_GAP_LOW && fabs(vEle.Eta())<=ECAL_GAP_HIGH) continue;
        if(doScaleCorr && (ele->r9 < 1.)){
            float eleSmear = 0.;

            float eleAbsEta   = fabs(vEle.Eta());
            float eleEt       = vEle.E() / cosh(eleAbsEta);
            bool  eleisBarrel = eleAbsEta < 1.4442;

            float eleR9Prime = ele->r9; // r9 corrections MC only
            if(eleisBarrel){
                      eleR9Prime = gR9EB->Eval(ele->r9);}
            else {
                      eleR9Prime = gR9EE->Eval(ele->r9);
            }

            double eleRamdom = gRandom->Gaus(0,1);
            eleSmear = eleCorr.getSmearingSigma(info->runNum, eleisBarrel, eleR9Prime, eleAbsEta, eleEt, 0., 0.);
            float eleSmearEP = eleCorr.getSmearingSigma(info->runNum, eleisBarrel, eleR9Prime, eleAbsEta, eleEt, 1., 0.);
            float eleSmearEM = eleCorr.getSmearingSigma(info->runNum, eleisBarrel, eleR9Prime, eleAbsEta, eleEt, -1., 0.);

            if(sigma==0){
              (vEle) *= 1. + eleSmear * eleRamdom;
            }else if(sigma==1){
              (vEle) *= 1. + eleSmearEP * eleRamdom;
            }else if(sigma==-1){
              (vEle) *= 1.  + eleSmearEM * eleRamdom;
            }
        }

        
//        if(fabs(ele->scEta) > VETO_ETA) continue;             // loose lepton |eta| cut
//        if(ele->scEt < VETO_PT)  continue;             // loose lepton pT cut
//        if(passEleLooseID(ele,info->rhoIso)) nLooseLep++;     // loose lepton selection
        if(fabs(vEle.Eta())    > VETO_ETA) continue;
        if(vEle.Pt()           < VETO_PT)  continue;
        if(passEleLooseID(ele, vEle, info->rhoIso)) nLooseLep++;

        if(nLooseLep>1) {  // extra lepton veto
          passSel=kFALSE;
          break;
        }

//        if(fabs(ele->scEta) > ETA_CUT && fabs(ele->eta) > ETA_CUT)       continue;  // lepton |eta| cut
//        if(ele->pt < PT_CUT && ele->scEt < PT_CUT)  	     continue;  // lepton pT cut
//        if(!passEleID(ele,info->rhoIso))     continue;  // lepton selection
        if(vEle.Pt()           < PT_CUT)     continue;  // lepton pT cut
        if(fabs(vEle.Eta())    > ETA_CUT)    continue;  // lepton |eta| cut
        if(!passEleID(ele, vEle, info->rhoIso))     continue;  // lepton selection

	if(!isEleTriggerObj(triggerMenu, ele->hltMatchBits, kFALSE, kFALSE)) continue;
        //if(!(ele->hltMatchBits[trigObjHLT])) continue;  // check trigger matching

	if(charge!=0 && ele->q!=charge) continue;  // check charge (if necessary)
        
	passSel=kTRUE;
        goodEle = ele;  
	vElefinal = vEle;
      }
      
      if(passSel) {
        
	/******** We have a W candidate! HURRAY! ********/
        Bool_t isBarrel = (fabs(vElefinal.Eta())<ETA_BARREL) ? kTRUE : kFALSE;

        Double_t corr=1;
        if(dataHLTEffFile && zeeHLTEffFile) {
//          Double_t effdata = dataHLTEff.getEff(vElefinal.Eta(), vElefinal.Pt());
//          Double_t effmc   = zeeHLTEff.getEff(vElefinal.Eta(), vElefinal.Pt());

	  char funcname[20];
	  sprintf(funcname, "fitfcn_%d", getEtaBinLabel(vElefinal.Eta()));
	  TF1 *fdt = (TF1*)f_hlt_data->Get(funcname);
	  TF1 *fmc = (TF1*)f_hlt_mc  ->Get(funcname);
	  Double_t effdata = fdt->Eval(TMath::Min(vElefinal.Pt(),119.0));
	  Double_t effmc   = fmc->Eval(TMath::Min(vElefinal.Pt(),119.0));
	  delete fdt;
	  delete fmc;

	  corr *= effdata/effmc;
        }
        if(dataGsfSelEffFile && zeeGsfSelEffFile) {
          Double_t effdata = dataGsfSelEff.getEff(vElefinal.Eta(), vElefinal.Pt()) * h_sys->GetBinContent(h_sys->GetXaxis()->FindBin(vElefinal.Eta()), h_sys->GetYaxis()->FindBin(vElefinal.Pt()));
          Double_t effmc   = zeeGsfSelEff.getEff(vElefinal.Eta(), vElefinal.Pt());
          corr *= effdata/effmc;
        }

        if(dataHLTEffFile && zeeHLTEffFile) {
          Double_t effdata = dataHLTEff.getEff(vElefinal.Eta(), vElefinal.Pt());
          Double_t effmc   = zeeHLTEff.getEff(vElefinal.Eta(), vElefinal.Pt());
          Double_t errdata = TMath::Max(dataHLTEff.getErrLow(vElefinal.Eta(), vElefinal.Pt()),dataHLTEff.getErrHigh(vElefinal.Eta(), vElefinal.Pt()));
          Double_t errmc   = TMath::Max(zeeHLTEff.getErrLow(vElefinal.Eta(), vElefinal.Pt()), zeeHLTEff.getErrHigh(vElefinal.Eta(), vElefinal.Pt()));
          Double_t err     = corr*sqrt(errdata*errdata/effdata/effdata+errmc*errmc/effmc/effmc);
          hHLTErr->Fill(vElefinal.Eta(),vElefinal.Pt(),err);
          if(isBarrel) hHLTErrB->Fill(vElefinal.Eta(),vElefinal.Pt(),err);
          else         hHLTErrE->Fill(vElefinal.Eta(),vElefinal.Pt(),err);
        }
        if(dataGsfSelEffFile && zeeGsfSelEffFile) {
          Double_t effdata = dataGsfSelEff.getEff(vElefinal.Eta(), vElefinal.Pt());
          Double_t effmc   = zeeGsfSelEff.getEff(vElefinal.Eta(), vElefinal.Pt());
          Double_t errdata = TMath::Max(dataGsfSelEff.getErrLow(vElefinal.Eta(), vElefinal.Pt()),dataGsfSelEff.getErrHigh(vElefinal.Eta(), vElefinal.Pt()));
          Double_t errmc   = TMath::Max(zeeGsfSelEff.getErrLow(vElefinal.Eta(), vElefinal.Pt()), zeeGsfSelEff.getErrHigh(vElefinal.Eta(), vElefinal.Pt()));
          Double_t err     = corr*sqrt(errdata*errdata/effdata/effdata+errmc*errmc/effmc/effmc);
          hGsfSelErr->Fill(vElefinal.Eta(),vElefinal.Pt(),err);
          if(isBarrel) hGsfSelErrB->Fill(vElefinal.Eta(),vElefinal.Pt(),err);
          else         hGsfSelErrE->Fill(vElefinal.Eta(),vElefinal.Pt(),err);
        }
        
	nSelv[ifile]+=weight;
	nSelCorrv[ifile]+=weight*corr;
	nSelCorrVarv[ifile]+=weight*weight*corr*corr;

  	if(isBarrel) { 
	  nSelBv[ifile]+=weight;
	  nSelBCorrv[ifile]+=weight*corr;
	  nSelBCorrVarv[ifile]+=weight*weight*corr*corr;
	  	
	} else { 
	  nSelEv[ifile]+=weight;
	  nSelECorrv[ifile]+=weight*corr;
	  nSelECorrVarv[ifile]+=weight*weight*corr*corr;
	}
      }
    }
    
    Double_t var=0, varB=0, varE=0;
    for(Int_t iy=0; iy<=hHLTErr->GetNbinsY(); iy++) {
      for(Int_t ix=0; ix<=hHLTErr->GetNbinsX(); ix++) {
        Double_t err;
	err=hHLTErr->GetBinContent(ix,iy);  var+=err*err;
        err=hHLTErrB->GetBinContent(ix,iy); varB+=err*err;
        err=hHLTErrE->GetBinContent(ix,iy); varE+=err*err;
      }
    }

    for(Int_t iy=0; iy<=hGsfSelErr->GetNbinsY(); iy++) {
      for(Int_t ix=0; ix<=hGsfSelErr->GetNbinsX(); ix++) {
        Double_t err;
	err=hGsfSelErr->GetBinContent(ix,iy);  var+=err*err;
	err=hGsfSelErrB->GetBinContent(ix,iy); varB+=err*err;
	err=hGsfSelErrE->GetBinContent(ix,iy); varE+=err*err;
      }
    }

    nSelCorrVarv[ifile]+=var;
    nSelBCorrVarv[ifile]+=varB;
    nSelECorrVarv[ifile]+=varE;
    
    // compute acceptances
    accv.push_back(nSelv[ifile]/nEvtsv[ifile]);   accErrv.push_back(sqrt(accv[ifile]*(1.+accv[ifile])/nEvtsv[ifile]));
    accBv.push_back(nSelBv[ifile]/nEvtsv[ifile]); accErrBv.push_back(sqrt(accBv[ifile]*(1.+accBv[ifile])/nEvtsv[ifile]));
    accEv.push_back(nSelEv[ifile]/nEvtsv[ifile]); accErrEv.push_back(sqrt(accEv[ifile]*(1.+accEv[ifile])/nEvtsv[ifile]));
    
    accCorrv.push_back(nSelCorrv[ifile]/nEvtsv[ifile]);   accErrCorrv.push_back(accCorrv[ifile]*sqrt(nSelCorrVarv[ifile]/nSelCorrv[ifile]/nSelCorrv[ifile] + 1./nEvtsv[ifile]));
    accBCorrv.push_back(nSelBCorrv[ifile]/nEvtsv[ifile]); accErrBCorrv.push_back(accBCorrv[ifile]*sqrt(nSelBCorrVarv[ifile]/nSelBCorrv[ifile]/nSelBCorrv[ifile] + 1./nEvtsv[ifile]));
    accECorrv.push_back(nSelECorrv[ifile]/nEvtsv[ifile]); accErrECorrv.push_back(accECorrv[ifile]*sqrt(nSelECorrVarv[ifile]/nSelECorrv[ifile]/nSelECorrv[ifile] + 1./nEvtsv[ifile]));
    
    delete infile;
    infile=0, eventTree=0;  
  }
  delete info;
  delete gen;
  delete electronArr;
  
    
  //--------------------------------------------------------------------------------------------------------------
  // Output
  //==============================================================================================================
   
  cout << "*" << endl;
  cout << "* SUMMARY" << endl;
  cout << "*--------------------------------------------------" << endl;
  if(charge== 0) cout << " W -> e nu"  << endl;
  if(charge==-1) cout << " W- -> e nu" << endl;
  if(charge== 1) cout << " W+ -> e nu" << endl;
  cout << "  pT > " << PT_CUT << endl;
  cout << "  |eta| < " << ETA_CUT << endl;
  cout << "  Barrel definition: |eta| < " << ETA_BARREL << endl;
  cout << "  Endcap definition: |eta| > " << ETA_ENDCAP << endl;
  cout << endl;
  
  for(UInt_t ifile=0; ifile<fnamev.size(); ifile++) {
    cout << "   ================================================" << endl;
    cout << "    Label: " << labelv[ifile] << endl;
    cout << "     File: " << fnamev[ifile] << endl;
    cout << endl;
    cout << "    *** Acceptance ***" << endl;
    cout << "     barrel: " << setw(12) << nSelBv[ifile] << " / " << nEvtsv[ifile] << " = " << accBv[ifile] << " +/- " << accErrBv[ifile];
    cout << "  ==eff corr==> " << accBCorrv[ifile] << " +/- " << accErrBCorrv[ifile] << endl;
    cout << "     endcap: " << setw(12) << nSelEv[ifile] << " / " << nEvtsv[ifile] << " = " << accEv[ifile] << " +/- " << accErrEv[ifile];
    cout << "  ==eff corr==> " << accECorrv[ifile] << " +/- " << accErrECorrv[ifile] << endl;
    cout << "      total: " << setw(12) << nSelv[ifile]  << " / " << nEvtsv[ifile] << " = " << accv[ifile]  << " +/- " << accErrv[ifile];
    cout << "  ==eff corr==> " << accCorrv[ifile]  << " +/- " << accErrCorrv[ifile] << endl;
    cout << endl;
  }
  
  char txtfname[100];
  sprintf(txtfname,"%s/sel.txt",outputDir.Data());
  ofstream txtfile;
  txtfile.open(txtfname);
  txtfile << "*" << endl;
  txtfile << "* SUMMARY" << endl;
  txtfile << "*--------------------------------------------------" << endl;
  if(charge== 0) txtfile << " W -> e nu"  << endl;
  if(charge==-1) txtfile << " W- -> e nu" << endl;
  if(charge== 1) txtfile << " W+ -> e nu" << endl;
  txtfile << "  pT > " << PT_CUT << endl;
  txtfile << "  |eta| < " << ETA_CUT << endl;
  txtfile << "  Barrel definition: |eta| < " << ETA_BARREL << endl;
  txtfile << "  Endcap definition: |eta| > " << ETA_ENDCAP << endl;
  txtfile << endl;
  
  for(UInt_t ifile=0; ifile<fnamev.size(); ifile++) {
    txtfile << "   ================================================" << endl;
    txtfile << "    Label: " << labelv[ifile] << endl;
    txtfile << "     File: " << fnamev[ifile] << endl;
    txtfile << endl;
    txtfile << "    *** Acceptance ***" << endl;
    txtfile << "     barrel: " << setw(12) << nSelBv[ifile] << " / " << nEvtsv[ifile] << " = " << accBv[ifile] << " +/- " << accErrBv[ifile];
    txtfile << "  ==eff corr==> " << accBCorrv[ifile] << " +/- " << accErrBCorrv[ifile] << endl;
    txtfile << "     endcap: " << setw(12) << nSelEv[ifile] << " / " << nEvtsv[ifile] << " = " << accEv[ifile] << " +/- " << accErrEv[ifile];
    txtfile << "  ==eff corr==> " << accECorrv[ifile] << " +/- " << accErrECorrv[ifile] << endl;
    txtfile << "      total: " << setw(12) << nSelv[ifile]  << " / " << nEvtsv[ifile] << " = " << accv[ifile]  << " +/- " << accErrv[ifile];
    txtfile << "  ==eff corr==> " << accCorrv[ifile]  << " +/- " << accErrCorrv[ifile] << endl;
    txtfile << endl;
  }
  txtfile.close();  
  
  cout << endl;
  cout << "  <> Output saved in " << outputDir << "/" << endl;    
  cout << endl;  
      
  gBenchmark->Show("computeAccSelWe"); 
}
