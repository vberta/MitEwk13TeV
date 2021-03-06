//================================================================================================
//
// Compute W->enu acceptance at supercluster level
//
//  * outputs results summary text file
//
//________________________________________________________________________________________________

#if !defined(__CINT__) //|| defined(__MAKECINT__)
#include <TROOT.h>                  // access to gROOT, entry point to ROOT system
#include <TSystem.h>                // interface to OS
#include <TFile.h>                  // file handle class
#include <TTree.h>                  // class to access ntuples
#include <TClonesArray.h>           // ROOT array class
#include <TBenchmark.h>             // class to track macro running statistics
#include <TH1D.h>                   // histogram class
#include <vector>                   // STL vector class
#include <iostream>                 // standard I/O
#include <iomanip>                  // functions to format standard I/O
#include <fstream>                  // functions for file I/O
#include <string>                   // C++ string class
#include <sstream>                  // class for parsing strings
#include <TLorentzVector.h>
#include "LHAPDF/LHAPDF.h"

#include "../Utils/MyTools.hh" // various helper functions

// define structures to read in ntuple
#include "BaconAna/DataFormats/interface/TEventInfo.hh"
#include "BaconAna/DataFormats/interface/TGenEventInfo.hh"
#include "BaconAna/DataFormats/interface/TGenParticle.hh"
#include "BaconAna/DataFormats/interface/TPhoton.hh"

//#include "../Utils/LeptonIDCuts.hh" // helper functions for lepton ID selection
#endif


//=== FUNCTION DECLARATIONS ======================================================================================

Double_t ecalEta(const Double_t eta, const Double_t z, const Double_t rho);


//=== MAIN MACRO ================================================================================================= 

void computeAccSCWe(const TString conf="we.conf",             // input file
                    const TString outputDir="why",	    // output directory
		    const Int_t   charge=-1 	    // 0 = inclusive, +1 = W+, -1 = W-
		    ) {
  gBenchmark->Start("computeAccSCWe");
  
  //--------------------------------------------------------------------------------------------------------------
  // Settings 
  //============================================================================================================== 
  
  const Double_t PT_CUT     = 25;
  const Double_t ETA_CUT    = 2.5;
  const Double_t ETA_BARREL = 1.4442;
  const Double_t ETA_ENDCAP = 1.566;
  
  const Int_t BOSON_ID  = 24;
  const Int_t LEPTON_ID = 11;
  
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
  
  // Data structures to store info from TTrees
  baconhep::TEventInfo *info = new baconhep::TEventInfo();
  baconhep::TGenEventInfo *gen  = new baconhep::TGenEventInfo();
  TClonesArray *photonArr = new TClonesArray("baconhep::TPhoton");
  TClonesArray *genPartArr = new TClonesArray("baconhep::TGenParticle");
  
  TFile *infile=0;
  TTree *eventTree=0;

  // Variables to store acceptances and uncertainties (per input file)
  vector<Double_t> nEvtsv, nSelv, nSelBv, nSelEv;
  vector<Double_t> accv, accBv, accEv;
  vector<Double_t> accErrv, accErrBv, accErrEv;

  LHAPDF::PDF* nomPdf = LHAPDF::mkPDF(292200);
  LHAPDF::PDF* testPdf = LHAPDF::mkPDF(13100);
  //LHAPDF::PDF* testPdf = LHAPDF::mkPDF(25100);

  //
  // loop through files
  //
  for(UInt_t ifile=0; ifile<fnamev.size(); ifile++) {  

    // Read input file and get the TTrees
    cout << "Processing " << fnamev[ifile] << " ..." << endl;
    infile = TFile::Open(fnamev[ifile]); 
    assert(infile);
  
    eventTree = (TTree*)infile->Get("Events"); assert(eventTree);  
    eventTree->SetBranchAddress("Info",              &info); TBranch *infoBr   = eventTree->GetBranch("Info");
    eventTree->SetBranchAddress("GenEvtInfo",         &gen); TBranch *genBr    = eventTree->GetBranch("GenEvtInfo");
    eventTree->SetBranchAddress("GenParticle", &genPartArr); TBranch *genPartBr  = eventTree->GetBranch("GenParticle");
    eventTree->SetBranchAddress("Photon",       &photonArr); TBranch *photonBr = eventTree->GetBranch("Photon");

    nEvtsv.push_back(0);
    nSelv.push_back(0);
    nSelBv.push_back(0);
    nSelEv.push_back(0);
    
    //
    // loop over events
    //
    //for(UInt_t ientry=0; ientry<eventTree->GetEntries(); ientry++) {
    for(UInt_t ientry=0; ientry<eventTree->GetEntries(); ientry++) {
      infoBr->GetEntry(ientry);
      genBr->GetEntry(ientry);
      genPartArr->Clear(); genPartBr->GetEntry(ientry);

      if (charge==-1 && toolbox::flavor(genPartArr, -BOSON_ID)!=LEPTON_ID) continue;
      if (charge==1 && toolbox::flavor(genPartArr, BOSON_ID)!=-LEPTON_ID) continue;
      if (charge==0 && fabs(toolbox::flavor(genPartArr, BOSON_ID))!=LEPTON_ID)  continue;

      TLorentzVector *vec=new TLorentzVector(0,0,0,0);
      TLorentzVector *lep1=new TLorentzVector(0,0,0,0);
      TLorentzVector *lep2=new TLorentzVector(0,0,0,0);
      toolbox::fillGen(genPartArr, BOSON_ID, vec, lep1, lep2,1);

      Int_t fid_1 = gen->id_1;//( id_1==0 ? 21 : id_1 );
      Int_t fid_2 = gen->id_2;//( id_2==0 ? 21 : id_2 );
      Double_t weight=gen->weight*testPdf->xfxQ(fid_1, gen->x_1, gen->scalePDF)*testPdf->xfxQ(fid_2, gen->x_2, gen->scalePDF)/(nomPdf->xfxQ(fid_1, gen->x_1, gen->scalePDF)*nomPdf->xfxQ(fid_2, gen->x_2, gen->scalePDF));
      nEvtsv[ifile]+=weight;  
      
      // good vertex requirement
      //if(!(info->hasGoodPV)) continue;

      photonArr->Clear();
      photonBr->GetEntry(ientry);
      for(Int_t i=0; i<photonArr->GetEntriesFast(); i++) {
        const baconhep::TPhoton *sc = (baconhep::TPhoton*)((*photonArr)[i]);
        
        // check ECAL gap
        if(fabs(sc->scEta)>=ETA_BARREL && fabs(sc->scEta)<=ETA_ENDCAP) continue;
        
        if(sc->scEt	   < PT_CUT)  continue;  // Supercluster ET cut corrected for PV
        if(fabs(sc->scEta) > ETA_CUT) continue;  // Supercluster |eta| cut
        
	Double_t dr      = (lep1) ? toolbox::deltaR(sc->scEta,sc->scPhi,lep1->Eta(),lep1->Phi()) : toolbox::deltaR(sc->scEta,sc->scPhi,lep1->Eta(),lep2->Phi());
	if(dr>0.5) continue;
        
	// ******** We have a W candidate! HURRAY! *******
        
	Bool_t isBarrel = (fabs(sc->scEta)<ETA_BARREL) ? kTRUE : kFALSE;
        
	nSelv[ifile]+=weight;
  	if(isBarrel) nSelBv[ifile]+=weight;
	else	     nSelEv[ifile]+=weight;
	
	break;
      }
    }
    
    // compute acceptances
    accv.push_back(nSelv[ifile]/nEvtsv[ifile]);   accErrv.push_back(sqrt(accv[ifile]*(1.-accv[ifile])/nEvtsv[ifile]));
    accBv.push_back(nSelBv[ifile]/nEvtsv[ifile]); accErrBv.push_back(sqrt(accBv[ifile]*(1.-accBv[ifile])/nEvtsv[ifile]));
    accEv.push_back(nSelEv[ifile]/nEvtsv[ifile]); accErrEv.push_back(sqrt(accEv[ifile]*(1.-accEv[ifile])/nEvtsv[ifile]));
    
    delete infile;
    infile=0, eventTree=0;  
  }
  delete info;
  delete gen;
  delete photonArr;
  
    
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
    cout << "     barrel: " << setw(12) << nSelBv[ifile] << " / " << nEvtsv[ifile] << " = " << accBv[ifile] << " +/- " << accErrBv[ifile] << endl;
    cout << "     endcap: " << setw(12) << nSelEv[ifile] << " / " << nEvtsv[ifile] << " = " << accEv[ifile] << " +/- " << accErrEv[ifile] << endl;
    cout << "      total: " << setw(12) << nSelv[ifile]  << " / " << nEvtsv[ifile] << " = " << accv[ifile]  << " +/- " << accErrv[ifile] << endl;
    cout << endl;
  }
  
  char txtfname[100];
  sprintf(txtfname,"%s/sc.txt",outputDir.Data());
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
    txtfile << "     barrel: " << setw(12) << nSelBv[ifile] << " / " << nEvtsv[ifile] << " = " << accBv[ifile] << " +/- " << accErrBv[ifile] << endl;
    txtfile << "     endcap: " << setw(12) << nSelEv[ifile] << " / " << nEvtsv[ifile] << " = " << accEv[ifile] << " +/- " << accErrEv[ifile] << endl;
    txtfile << "      total: " << setw(12) << nSelv[ifile]  << " / " << nEvtsv[ifile] << " = " << accv[ifile]  << " +/- " << accErrv[ifile] << endl;
    txtfile << endl;
  }
  txtfile.close();  
  
  cout << endl;
  cout << "  <> Output saved in " << outputDir << "/" << endl;    
  cout << endl;  

  gBenchmark->Show("computeAccSCWe"); 
}


//=== FUNCTION DEFINITIONS ======================================================================================

//--------------------------------------------------------------------------------------------------
Double_t ecalEta(const Double_t eta, const Double_t z, const Double_t rho) {
  // From https://twiki.cern.ch/twiki/bin/view/CMS/SWGuideEcalRecoClustering
  
  const Double_t R_ECAL            = 136.5;
  const Double_t Z_ENDCAP          = 328.0;
  const Double_t ETA_BARREL_ENDCAP = 1.479;
  const Double_t pi                = 3.14159265358979;

  if(eta!= 0.) {
    Double_t theta = 0.0;
    Double_t zecal = (R_ECAL - rho)*sinh(eta) + z;
      
    if(zecal != 0.0) theta = atan(R_ECAL/zecal);
    if(theta<0.0) theta = theta + pi;

    Double_t detEta = -log(tan(0.5*theta));
      
    if(fabs(detEta) > ETA_BARREL_ENDCAP) {
      Double_t zend = Z_ENDCAP;
      if(eta<0.0) zend = -zend;
      Double_t zlen = zend - z;
      Double_t rr = zlen/sinh(eta);
      theta = atan((rr+rho)/zend);
      if(theta<0.0) theta = theta + pi;
      detEta = - log(tan(0.5*theta));
    }
    return detEta;
      
  } else {
    return eta;
  }
}
