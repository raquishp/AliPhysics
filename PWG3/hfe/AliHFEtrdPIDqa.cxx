/**************************************************************************
* Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
*                                                                        *
* Author: The ALICE Off-line Project.                                    *
* Contributors are mentioned in the code where appropriate.              *
*                                                                        *
* Permission to use, copy, modify and distribute this software and its   *
* documentation strictly for non-commercial purposes is hereby granted   *
* without fee, provided that the above copyright notice appears in all   *
* copies and that both the copyright notice and this permission notice   *
* appear in the supporting documentation. The authors make no claims     *
* about the suitability of this software for any purpose. It is          *
* provided "as is" without express or implied warranty.                  *
**************************************************************************/
//
// QA class for TRD PID
// Plot Pion Efficiency at x electron efficiency
// Calculate the threshold parametrisation and save
// them in a root file
//
// Author:
//   Markus Fasel <M.Fasel@gsi.de>
//
#include <TAxis.h>
#include <TClass.h>
#include <TCanvas.h>
#include <TF1.h>
#include <TFile.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <THnSparse.h>
#include <TH1.h>
#include <TH2.h>
#include <TIterator.h>
#include <TLegend.h>
#include <TList.h>
#include <TMath.h>
#include <TObjArray.h>
#include <TString.h>

#include "AliAODTrack.h"
#include "AliAODPid.h"
#include "AliESDtrack.h"
#include "AliHFEtrdPIDqa.h"
#include "AliHFEtools.h"
#include "AliHFEpidTRD.h"
#include "AliLog.h"

ClassImp(AliHFEtrdPIDqa)

const Double_t AliHFEtrdPIDqa::fgkElectronEff[kNElectronEffs] = {0.7,0.75, 0.8, 0.85, 0.9, 0.95};
//_______________________________________________________________
// Definition of the common binning 
const Int_t AliHFEtrdPIDqa::fgkNBinsCommon[kQuantitiesCommon] = {
  AliPID::kSPECIES + 1,         // species
  40,                           // p-bins
  AliESDtrack::kTRDnPlanes + 1 // tracklets including 0
};
const Double_t AliHFEtrdPIDqa::fgkMinBinCommon[kQuantitiesCommon] = {
  -1,      // species
  0.1,     // p-bins
  0        // tracklets including 0
};

const Double_t AliHFEtrdPIDqa::fgkMaxBinCommon[kQuantitiesCommon] = {
  AliPID::kSPECIES,               // species
  10.,                            // p-bins
  AliESDtrack::kTRDnPlanes + 1    // tracklets including 0
};
//_______________________________________________________________

//__________________________________________________________________
AliHFEtrdPIDqa::AliHFEtrdPIDqa():
  TNamed("trdPIDqa", ""),
  fTRDpid(NULL),
  fLikeTRD(NULL),
  fQAtrack(NULL),
  fQAdEdx(NULL),
  fTRDtruncMean(NULL),
  fPionEfficiencies(NULL),
  fProtonEfficiencies(NULL),
  fKaonEfficiencies(NULL),
  fThresholds(NULL)
{
  //
  // Default Constructor
  //
}

//__________________________________________________________________
AliHFEtrdPIDqa::AliHFEtrdPIDqa(const Char_t *name):
  TNamed(name, ""),
  fTRDpid(NULL),
  fLikeTRD(NULL),
  fQAtrack(NULL),
  fQAdEdx(NULL),
  fTRDtruncMean(NULL),
  fPionEfficiencies(NULL),
  fProtonEfficiencies(NULL),
  fKaonEfficiencies(NULL),
  fThresholds(NULL)
{
  //
  // Main Constructor
  //
}

//__________________________________________________________________
AliHFEtrdPIDqa::AliHFEtrdPIDqa(const AliHFEtrdPIDqa &ref):
  TNamed(ref),
  fTRDpid(NULL),
  fLikeTRD(NULL),
  fQAtrack(NULL),
  fQAdEdx(NULL),
  fTRDtruncMean(NULL),
  fPionEfficiencies(NULL),
  fProtonEfficiencies(NULL),
  fKaonEfficiencies(NULL),
  fThresholds(NULL)
{
  //
  // Copy constructor
  //
  ref.Copy(*this);
}

//__________________________________________________________________
AliHFEtrdPIDqa &AliHFEtrdPIDqa::operator=(const AliHFEtrdPIDqa &ref){
  //
  // Assignment operator
  //
  if(this != &ref)
    ref.Copy(*this);
  return *this;
}

//__________________________________________________________________
AliHFEtrdPIDqa::~AliHFEtrdPIDqa(){
  //
  // Destructor
  //
  if(fTRDpid) delete fTRDpid;
  if(fLikeTRD) delete fLikeTRD;
  if(fQAtrack) delete fQAtrack;
  if(fQAdEdx) delete fQAdEdx;
  if(fTRDtruncMean) delete fTRDtruncMean;
  
  if(fPionEfficiencies) delete fPionEfficiencies;
  if(fProtonEfficiencies) delete fProtonEfficiencies;
  if(fKaonEfficiencies) delete fKaonEfficiencies;
}

//__________________________________________________________________
void AliHFEtrdPIDqa::Copy(TObject &ref) const{
  //
  // Copies content of this object into object ref
  //
  TNamed::Copy(ref);

  AliHFEtrdPIDqa &target = dynamic_cast<AliHFEtrdPIDqa &>(ref);
  target.fTRDpid = fTRDpid;
  target.fLikeTRD = dynamic_cast<THnSparseF *>(fLikeTRD->Clone());
  target.fQAtrack = dynamic_cast<THnSparseF *>(fQAtrack->Clone());
  target.fQAdEdx = dynamic_cast<THnSparseF *>(fQAdEdx->Clone());
  target.fTRDtruncMean = dynamic_cast<THnSparseF *>(fTRDtruncMean->Clone());
}

//__________________________________________________________________
Long64_t AliHFEtrdPIDqa::Merge(TCollection *coll){
  //
  // Merge objects
  //
  if(!coll) return 0;
  if(coll->IsEmpty()) return 1;
  
  TIterator *it = coll->MakeIterator();
  TObject *o = NULL;
  Long64_t count = 0;
  while((o = it->Next())){
    AliHFEtrdPIDqa *refQA = dynamic_cast<AliHFEtrdPIDqa *>(o);
    if(!refQA) continue;

    if(fLikeTRD) fLikeTRD->Add(refQA->fLikeTRD);
    if(fQAtrack) fQAtrack->Add(refQA->fQAtrack);
    if(fQAdEdx) fQAdEdx->Add(refQA->fQAdEdx);
    if(fTRDtruncMean) fTRDtruncMean->Add(refQA->fTRDtruncMean);
    count++;
  }
  delete it;
  return count+1;
}

//__________________________________________________________________
void AliHFEtrdPIDqa::Init(){
  //
  // Initialize Object
  //

  CreateLikelihoodHistogram();
  CreateQAHistogram();
  CreatedEdxHistogram();
  CreateHistoTruncatedMean();

  fTRDpid = new AliHFEpidTRD("QAtrdPID");
}

//__________________________________________________________________
void AliHFEtrdPIDqa::CreateLikelihoodHistogram(){
  //
  // Create Histogram for TRD Likelihood Studies
  //
  Int_t nbins[kQuantitiesLike]; memcpy(nbins, fgkNBinsCommon, sizeof(Int_t) * kQuantitiesCommon);
  nbins[kElectronLike] = 100;
  Double_t binMin[kQuantitiesLike]; memcpy(binMin, fgkMinBinCommon, sizeof(Double_t) * kQuantitiesCommon);
  Double_t binMax[kQuantitiesLike]; memcpy(binMax, fgkMaxBinCommon, sizeof(Double_t) * kQuantitiesCommon);
  binMin[kElectronLike] = 0.;      
  binMax[kElectronLike] = 1.;

  fLikeTRD = new THnSparseF("fLikeTRD","TRD Likelihood Studies", kQuantitiesLike, nbins, binMin, binMax);
  Double_t *binLog = AliHFEtools::MakeLogarithmicBinning(nbins[kP], binMin[kP], binMax[kP]);
  fLikeTRD->GetAxis(kP)->Set(nbins[kP], binLog);
  delete[] binLog;
}

//__________________________________________________________________
void AliHFEtrdPIDqa::CreateQAHistogram(){
  //
  // Create Histogram for Basic TRD PID QA
  //
  AliDebug(1, "Called");
  Int_t nbins[kQuantitiesQA]; memcpy(nbins, fgkNBinsCommon, sizeof(Int_t) * kQuantitiesCommon);
  nbins[kNonZeroTrackletCharge] = AliESDtrack::kTRDnPlanes + 1;
  nbins[kNClusters] = 200;
  Double_t binMin[kQuantitiesQA]; memcpy(binMin, fgkMinBinCommon, sizeof(Double_t) * kQuantitiesCommon);
  binMin[kNonZeroTrackletCharge] = 0.;      
  binMin[kNClusters] = 0.;      
  Double_t binMax[kQuantitiesQA]; memcpy(binMax, fgkMaxBinCommon, sizeof(Double_t) * kQuantitiesCommon);
  binMax[kNonZeroTrackletCharge] = AliESDtrack::kTRDnPlanes + 1.;
  binMax[kNClusters] = 200.;

  fQAtrack = new THnSparseF("fQAtrack","TRD QA Histogram", kQuantitiesQA, nbins, binMin, binMax);
  Double_t *binLog = AliHFEtools::MakeLogarithmicBinning(nbins[kP], binMin[kP], binMax[kP]);
  fQAtrack->GetAxis(kP)->Set(nbins[kP], binLog);
  delete[] binLog;
}

//__________________________________________________________________
void AliHFEtrdPIDqa::CreatedEdxHistogram(){
  //
  // Create QA histogram for dEdx investigations
  //
  AliDebug(1, "Called");
  Int_t nbins[kQuantitiesdEdx]; memcpy(nbins, fgkNBinsCommon, sizeof(Int_t) * kQuantitiesCommon);
  nbins[kdEdx] = 100;
  nbins[kNclusters] = 261;
  nbins[kNonZeroSlices] = 9; 
  Double_t binMin[kQuantitiesdEdx]; memcpy(binMin, fgkMinBinCommon, sizeof(Double_t) * kQuantitiesCommon);
  binMin[kdEdx] = 0.;     
  binMin[kNclusters] = 0;
  Double_t binMax[kQuantitiesdEdx]; memcpy(binMax, fgkMaxBinCommon, sizeof(Double_t) * kQuantitiesCommon);
  binMax[kdEdx] = 100000.;
  binMax[kNclusters] = 260.;
  binMax[kNonZeroSlices] = 8.;

  fQAdEdx = new THnSparseF("fQAdEdx","TRD summed dEdx", kQuantitiesdEdx, nbins, binMin, binMax);
  Double_t *binLog = AliHFEtools::MakeLogarithmicBinning(nbins[kP], binMin[kP], binMax[kP]);
  fQAdEdx->GetAxis(kP)->Set(nbins[kP], binLog);
  delete[] binLog;
}

//__________________________________________________________________
void AliHFEtrdPIDqa::CreateHistoTruncatedMean(){
  //
  // Create Histogram for Basic TRD PID QA
  //
  AliDebug(1, "Called");
  Int_t nbins[kQuantitiesTruncMean]; memcpy(nbins, fgkNBinsCommon, sizeof(Int_t) * kQuantitiesCommon);
  nbins[kTPCdEdx] = 600;
  nbins[kTRDdEdxMethod1] = 1000;
  nbins[kTRDdEdxMethod2] = 1000;
  Double_t binMin[kQuantitiesTruncMean]; memcpy(binMin, fgkMinBinCommon, sizeof(Double_t) * kQuantitiesCommon);
  binMin[kTPCdEdx] = 0.;      
  binMin[kTRDdEdxMethod1] = 0.;      
  binMin[kTRDdEdxMethod2] = 0.;      
  Double_t binMax[kQuantitiesTruncMean]; memcpy(binMax, fgkMaxBinCommon, sizeof(Double_t) * kQuantitiesCommon);
  binMax[kTPCdEdx] = 600;
  binMax[kTRDdEdxMethod1] = 20000.;
  binMax[kTRDdEdxMethod2] = 20000.;

  fTRDtruncMean = new THnSparseF("fTRDtruncMean","TRD TruncatedMean studies", kQuantitiesTruncMean, nbins, binMin, binMax);
  Double_t *binLog = AliHFEtools::MakeLogarithmicBinning(nbins[kP], binMin[kP], binMax[kP]);
  fTRDtruncMean->GetAxis(kP)->Set(nbins[kP], binLog);
  delete[] binLog;
}


//__________________________________________________________________
void AliHFEtrdPIDqa::ProcessTracks(TObjArray * const tracks, Int_t species){
  //
  // Process Electron Tracks
  //
  if(species < -1 || species >= AliPID::kSPECIES) return;
  TIterator *it = tracks->MakeIterator();
  AliVTrack *track = NULL;
  while((track = dynamic_cast<AliVTrack *>(it->Next()))){
    if(track) ProcessTrack(track, species);
  }
  delete it;
}

//__________________________________________________________________
void AliHFEtrdPIDqa::ProcessTrack(AliVTrack *track, Int_t species){
  //
  // Process Single Track
  //
  if(TString(track->IsA()->GetName()).CompareTo("AliESDtrack") == 0)
    ProcessTrackESD(dynamic_cast<AliESDtrack *>(track), species);
  else if(TString(track->IsA()->GetName()).CompareTo("AliAODTrack") == 0)
    ProcessTrackAOD(dynamic_cast<AliAODTrack *>(track), species);
}


//__________________________________________________________________
void AliHFEtrdPIDqa::ProcessTrackESD(AliESDtrack *track, Int_t species){
  //
  // Process single ESD track
  //
  if((track->GetStatus() & AliESDtrack::kTRDout) == 0) return;  // require TRD track
  FillTRDLikelihoods(track, species);
  FillTRDQAplots(track, species);
}

//__________________________________________________________________
void AliHFEtrdPIDqa::ProcessTrackAOD(AliAODTrack * const track, Int_t /*species*/){
  //
  // Process single AOD track
  // AOD PID object required
  //
  AliAODPid *trackPID = track->GetDetPid();
  if(!trackPID) return;

}

//__________________________________________________________________
void AliHFEtrdPIDqa::FillTRDLikelihoods(AliESDtrack *track, Int_t species){
  //
  // Fill TRD Likelihood Histogram
  //
  Double_t trdLike[AliPID::kSPECIES];
  track->GetTRDpid(trdLike);
  const AliExternalTrackParam *outerPars = track->GetOuterParam();

  Double_t quantities[kQuantitiesLike]; memset(quantities, 0, sizeof(Double_t) * kQuantitiesLike);
  // we store:
  // species
  // p
  // ntracklets
  // Electron Likelihood
  quantities[kSpecies] = species;
  quantities[kP] = outerPars ? outerPars->P() : track->P();
  quantities[kNTracklets] = track->GetTRDntrackletsPID();
  quantities[kElectronLike] = trdLike[AliPID::kElectron];

  fLikeTRD->Fill(quantities);
}

//__________________________________________________________________
void AliHFEtrdPIDqa::FillTRDQAplots(AliESDtrack *track, Int_t species){
  //
  // Fill QA Plots containing further information
  //
  const AliExternalTrackParam *outerPars = track->GetOuterParam();

  Double_t quantitiesQA[kQuantitiesQA], quantitiesdEdx[kQuantitiesdEdx], quantitiesTruncMean[kQuantitiesTruncMean];
  // we store:
  // 1. QA
  // species
  // p
  // ntracklets
  // Non-zero tracklet charges
  // Number of clusters / full track
  // 2. dEdx
  // species
  // p
  // ntracklets
  // dEdx
  // 3. Truncated Mean
  // ...
  // TPC dEdx
  // TRD dEdx Method 1
  // TRD dEdx Method 2
  quantitiesQA[kSpecies]  = quantitiesdEdx[kSpecies] 
                          = quantitiesTruncMean[kSpecies] 
                          = species;
  quantitiesQA[kP]  = quantitiesdEdx[kP] 
                    = quantitiesTruncMean[kP]
                    = outerPars ? outerPars->P() : track->P();
  quantitiesQA[kNTracklets] = quantitiesdEdx[kNTracklets] 
                            = quantitiesTruncMean[kNTracklets]
                            = track->GetTRDntrackletsPID();
  quantitiesQA[kNClusters] = quantitiesdEdx[kNclusters] = track->GetTRDncls();
  

  Double_t dEdxSum = 0., qSlice = 0.;
  // remove the last slice from the histogram
  Int_t ntrackletsNonZero = 0, nSlices = track->GetNumberOfTRDslices() - 1, nSlicesNonZero = 0;
  for(Int_t iplane = 0; iplane < AliESDtrack::kTRDnPlanes; iplane++){
    dEdxSum = 0.;
    for(Int_t islice = 0; islice < nSlices; islice++){
      qSlice = track->GetTRDslice(iplane, islice);
      if(qSlice > 1e-1){
        // cut out 0 slices
        nSlicesNonZero++;
        dEdxSum += qSlice;
      }
    }
    quantitiesdEdx[kNonZeroSlices] = nSlicesNonZero;
    quantitiesdEdx[kdEdx] = dEdxSum;
    if(dEdxSum) ntrackletsNonZero++;
    // Fill dEdx histogram
    if(dEdxSum > 1e-1) fQAdEdx->Fill(quantitiesdEdx); // Cut out 0 entries
  }
  quantitiesQA[kNonZeroTrackletCharge] = ntrackletsNonZero;
  fQAtrack->Fill(quantitiesQA);

  quantitiesTruncMean[kTPCdEdx] = track->GetTPCsignal();
  quantitiesTruncMean[kTRDdEdxMethod1] = fTRDpid->GetTRDSignalV1(track);
  quantitiesTruncMean[kTRDdEdxMethod2] = fTRDpid->GetTRDSignalV2(track);
  fTRDtruncMean->Fill(quantitiesTruncMean);
}

/////////////////////////////////////////////////////////
//
// Code for Post Processing
//
// //////////////////////////////////////////////////////

//__________________________________________________________________
void AliHFEtrdPIDqa::FinishAnalysis(){
  //
  // Finish Analysis:
  // Calculate Electron Efficiency for ntracklets = 4...6
  // Calculate thresholds for ntracklets = 4...6
  //
  
  if(!fPionEfficiencies){
    fPionEfficiencies = new TList;
    fPionEfficiencies->SetName("pionEfficiencies");
  }
  if(!fProtonEfficiencies){
    fProtonEfficiencies = new TList;
    fProtonEfficiencies->SetName("protonEfficiencies");
  }
  if(!fThresholds){
    fThresholds = new TList;
    fThresholds->SetName("thresholds");
  }

  for(Int_t itr = 4; itr <= 6; itr++){
    printf("========================================\n");
    printf("Analysing %d trackltes\n", itr);
    printf("========================================\n");
    AnalyseNTracklets(itr);
  }
}

//__________________________________________________________________
void AliHFEtrdPIDqa::StoreResults(const Char_t *filename){
  //
  // Store histos into a root file
  //
  TFile *outfile = new TFile(filename, "RECREATE");
  outfile->cd();
  fPionEfficiencies->Clone()->Write(fPionEfficiencies->GetName(), kSingleKey);
  fProtonEfficiencies->Clone()->Write(fProtonEfficiencies->GetName(), kSingleKey);
  fThresholds->Clone()->Write(fThresholds->GetName(), kSingleKey);
  outfile->Close();
  delete outfile;
}

//__________________________________________________________________
void AliHFEtrdPIDqa::SaveThresholdParameters(const Char_t *name){
  //
  // Fit the threshold histograms with the given parametrisation
  // and store the TF1 in the file
  //

  if(!fThresholds){
    AliError("Threshold histograms have to be created first");
    return;
  }

  printf("========================================\n");
  printf("Calculating threshold parameters\n");
  printf("========================================\n");

  TList *outlist = new TList;
  outlist->SetName("thresholdTRD");
  
  TGraph *threshhist = NULL;
  TF1 *threshparam = NULL;
  TList *lHistos = NULL, *lFormulas = NULL;
  for(Int_t itracklet = 4; itracklet <= 6; itracklet++){
  
    printf("-------------------------------\n");
    printf("Processing %d tracklets\n", itracklet);
    printf("-------------------------------\n");

    lHistos = dynamic_cast<TList *>(fThresholds->FindObject(Form("%dTracklets", itracklet)));
    if(!lHistos){
      AliError(Form("Threshold histograms for the case %d tracklets not found", itracklet));
      continue;
    }
    lFormulas = new TList;
    lFormulas->SetName(Form("%dTracklets", itracklet));
    outlist->Add(lFormulas);
    
    for(Int_t ieff = 0; ieff <  kNElectronEffs; ieff++){
      
      printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
      printf("Processing Electron Efficiency %f\n", fgkElectronEff[ieff]);
      printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

      threshhist = dynamic_cast<TGraph *>(lHistos->FindObject(Form("eff%d", static_cast<Int_t>(fgkElectronEff[ieff] * 100))));
      threshparam = MakeThresholds(threshhist);
      threshparam->SetName(Form("thresh_%d_%d", itracklet, static_cast<Int_t>(fgkElectronEff[ieff] * 100)));
    }
  }

  // store the output
  TFile *outfile = new TFile(name, "RECREATE");
  outfile->cd();
  outlist->Write(outlist->GetName(), kSingleKey);
  outfile->Close();
  delete outfile;
}

//__________________________________________________________________
void AliHFEtrdPIDqa::AnalyseNTracklets(Int_t nTracklets){
  //
  // Calculate Pion Efficiency, Proton Efficiency and Kaon Efficiency at discrete
  // elPion Efficiency, Proton Efficiency and Kaon Efficiency at discrete
  // electron efficiencies
  //
  Int_t binTracklets = fLikeTRD->GetAxis(kNTracklets)->FindBin(nTracklets);
  fLikeTRD->GetAxis(kNTracklets)->SetRange(binTracklets, binTracklets);
  
  Int_t binElectrons = fLikeTRD->GetAxis(kSpecies)->FindBin(AliPID::kElectron); 
  AliDebug(1, Form("BinElectrons %d", binElectrons));
  Int_t binPions = fLikeTRD->GetAxis(kSpecies)->FindBin(AliPID::kPion);
  AliDebug(1, Form("BinPions %d", binPions));
  Int_t binProtons =  fLikeTRD->GetAxis(kSpecies)->FindBin(AliPID::kProton);
  AliDebug(1, Form("BinProtons %d", binProtons));
  fLikeTRD->GetAxis(kSpecies)->SetRange(binElectrons, binElectrons);
  TH2 *likeElectron = fLikeTRD->Projection(kElectronLike, kP);
  likeElectron->SetName("likeElectron");
  fLikeTRD->GetAxis(kSpecies)->SetRange(binPions, binPions);
  TH2 *likePion = fLikeTRD->Projection(kElectronLike, kP);
  likePion->SetName("likePion");
  fLikeTRD->GetAxis(kSpecies)->SetRange(binProtons, binProtons);
  TH2 *likeProton = fLikeTRD->Projection(kElectronLike, kP);
  likeProton->SetName("likeProton");
  
  // Undo ranges
  fLikeTRD->GetAxis(kSpecies)->SetRange(0, fLikeTRD->GetAxis(kSpecies)->GetNbins());
  fLikeTRD->GetAxis(kNTracklets)->SetRange(0, fLikeTRD->GetAxis(kNTracklets)->GetNbins());

  // Prepare List for output
  TList *listPions = new TList; listPions->SetName(Form("%dTracklets", nTracklets));
  TList *listProtons = new TList; listProtons->SetName(Form("%dTracklets", nTracklets));
  TList *listThresholds = new TList; listThresholds->SetName(Form("%dTracklets", nTracklets));
  fPionEfficiencies->Add(listPions);
  fProtonEfficiencies->Add(listProtons);
  fThresholds->Add(listThresholds);

  TH1 *probsEl = NULL, *probsPi = NULL, *probsPr = NULL;
  TGraphErrors *effPi = NULL, *effPr = NULL; TGraph *thresholds = NULL;
  Double_t p = 0, dp = 0;
  Int_t threshbin = 0;
  Double_t noElEff[2]; // value and error
  for(Int_t ieff = 0; ieff < kNElectronEffs; ieff++){
    printf("-----------------------------------------\n");
    printf("Doing Electron Efficiency %f\n", fgkElectronEff[ieff]);
    printf("-----------------------------------------\n");
    effPi = new TGraphErrors(likeElectron->GetXaxis()->GetNbins());
    effPi->SetName(Form("eff%d", static_cast<Int_t >(fgkElectronEff[ieff] * 100)));
    effPr = new TGraphErrors(likeElectron->GetXaxis()->GetNbins());
    effPr->SetName(Form("eff%d", static_cast<Int_t >(fgkElectronEff[ieff] * 100)));
    thresholds = new TGraph(likeElectron->GetXaxis()->GetNbins());
    thresholds->SetName(Form("eff%d", static_cast<Int_t >(fgkElectronEff[ieff] * 100)));
  
    // Add to lists
    listPions->Add(effPi);
    listProtons->Add(effPr);
    listThresholds->Add(thresholds);

    for(Int_t imom = 1; imom <= likeElectron->GetXaxis()->GetLast(); imom++){
      p = likeElectron->GetXaxis()->GetBinCenter(imom);
      dp = likeElectron->GetXaxis()->GetBinWidth(imom)/2;

      probsEl = likeElectron->ProjectionY("el", imom);
      if(!probsEl->GetEntries()) continue;
      probsEl->Scale(1./probsEl->Integral());
      probsPi = likePion->ProjectionY("pi", imom);
      if(!probsPi->GetEntries()) continue;
      probsPi->Scale(1./probsPi->Integral());
      probsPr = likeProton->ProjectionY("pr", imom);
      if(!probsPr->GetEntries()) continue;
      probsPr->Scale(1./probsPr->Integral());
      AliDebug(1, Form("Calculating Values for p = %f", p));

      // Calculare threshold we need to achive the x% electron Efficiency
      threshbin = GetThresholdBin(probsEl, fgkElectronEff[ieff]);
      thresholds->SetPoint(imom - 1, p, probsEl->GetXaxis()->GetBinCenter(threshbin));
      AliDebug(1, Form("threshold %d|%f", threshbin, probsEl->GetXaxis()->GetBinCenter(threshbin)));

      // Calculate non-electronEfficiency and error
      CalculateEfficiency(probsPi, threshbin, noElEff);
      AliDebug(1, Form("Pion Efficiency %f", noElEff[0]));
      effPi->SetPoint(imom - 1, p, noElEff[0]);
      effPi->SetPointError(imom - 1, dp, noElEff[1]);
      CalculateEfficiency(probsPr, threshbin, noElEff);
      effPr->SetPoint(imom - 1, p, noElEff[0]);
      effPr->SetPointError(imom - 1, dp, noElEff[1]);
      AliDebug(1, Form("Proton Efficiency %f", noElEff[0]));
 
      // cleanup
      delete probsEl;
      delete probsPi;
      delete probsPr;
    }
  }

  // remove temporary histograms
  delete likeElectron;
  delete likePion;
  delete likeProton;
}

//__________________________________________________________________
Int_t AliHFEtrdPIDqa::GetThresholdBin(TH1 * const input, Double_t eff){
  //
  // Calculate the threshold bin  
  //
  Double_t integralEff = 0.;
  Int_t currentBin = 0;
  for(Int_t ibin = input->GetXaxis()->GetLast(); ibin >= input->GetXaxis()->GetFirst(); ibin--){
    currentBin = ibin;
    integralEff += input->GetBinContent(ibin);
    if(integralEff >= eff){
      // we found the matching bin, break the loop
      break;
    }
  }
  return currentBin;
}

//__________________________________________________________________
Bool_t AliHFEtrdPIDqa::CalculateEfficiency(TH1 * const input, Int_t threshbin, Double_t *par){
  // 
  // Calculate non-electron efficiency
  //
  Double_t integralEff = 0; 
  for(Int_t ibin = threshbin; ibin <= input->GetXaxis()->GetLast(); ibin++) 
    integralEff += input->GetBinContent(ibin);
  par[0] = integralEff;

  // @TODO: Error calculation
  par[1] = 0;

  return kTRUE;
}

//__________________________________________________________________
void AliHFEtrdPIDqa::DrawTracklet(Int_t itracklet){
  //
  // Draw efficiencies and threshold as function of p
  //
  if(!(fPionEfficiencies && fProtonEfficiencies && fThresholds)){
    AliError("No graphs to draw available");  
    return;
  }

  TList *lpions = dynamic_cast<TList *>(fPionEfficiencies->FindObject(Form("%dTracklets", itracklet))); 
  TList *lprotons = dynamic_cast<TList *>(fProtonEfficiencies->FindObject(Form("%dTracklets", itracklet))); 
  
  TList *lthresholds = dynamic_cast<TList *>(fThresholds->FindObject(Form("%dTracklets", itracklet))); 

  TGraphErrors *pi, *pr;
  TGraph *tr;
  TLegend *leg;
  TCanvas *c1 = new TCanvas(Form("tracklet%d", itracklet), Form("Tracklet %d", itracklet), 1024, 768);
  c1->Divide(3,2);
  for(Int_t ieff = 0; ieff < kNElectronEffs; ieff++){
    c1->cd(ieff + 1);
    leg = new TLegend(0.6, 0.7, 0.89, 0.89);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    pi = dynamic_cast<TGraphErrors *>(lpions->FindObject(Form("eff%d", static_cast<Int_t>(fgkElectronEff[ieff] * 100))));
    pr = dynamic_cast<TGraphErrors *>(lprotons->FindObject(Form("eff%d", static_cast<Int_t>(fgkElectronEff[ieff] * 100))));
    tr = dynamic_cast<TGraph *>(lthresholds->FindObject(Form("eff%d", static_cast<Int_t>(fgkElectronEff[ieff] * 100))));

    // Define nice plot, draw
    // Axis Title
    pi->GetXaxis()->SetTitle("p / GeV/c");
    pi->GetYaxis()->SetTitle("Efficiency");
    pr->GetXaxis()->SetTitle("p / GeV/c");
    pr->GetYaxis()->SetTitle("Efficiency");
    tr->GetXaxis()->SetTitle("p / GeV/c");
    tr->GetYaxis()->SetTitle("Efficiency");
    // Axis Range
    pi->GetYaxis()->SetRangeUser(0., 1.);
    pr->GetYaxis()->SetRangeUser(0., 1.);
    tr->GetYaxis()->SetRangeUser(0., 1.);
    // Marker
    pi->SetMarkerColor(kRed);
    pi->SetMarkerStyle(20);
    pr->SetMarkerColor(kBlue);
    pr->SetMarkerStyle(21);
    tr->SetMarkerColor(kBlack);
    tr->SetMarkerStyle(22);
    // Title
    pi->SetTitle(Form ("%.2f Electron Efficiency", fgkElectronEff[ieff]));
    pr->SetTitle(Form ("%.2f Electron Efficiency", fgkElectronEff[ieff]));
    tr->SetTitle(Form ("%.2f Electron Efficiency", fgkElectronEff[ieff]));
    // Draw
    pi->Draw("ape"); pr->Draw("pesame"); tr->Draw("psame");

    // Add entries to legend
    leg->AddEntry(pi, "Pion Efficiency", "lp");
    leg->AddEntry(pr, "Proton Efficiency", "lp");
    leg->AddEntry(tr, "Thresholds", "lp");
    leg->Draw();
    c1->Update();
  }
}

//__________________________________________________________________
TF1 *AliHFEtrdPIDqa::MakeThresholds(TGraph *threshist){
  //
  // Create TF1 containing the threshold parametrisation
  //

  TF1 *threshparam = new TF1("thresh", "1-[0]-[1]*x-[2]*TMath::Exp(-[3]*x)", 0.1, 10);
  threshist->Fit(threshparam, "NE", "", 0, 10);
  return threshparam;
}

//__________________________________________________________________
void AliHFEtrdPIDqa::ClearLists(){
  //
  // Clear lists for particle efficiencies and thresholds
  //
  if(fPionEfficiencies){
    fPionEfficiencies->Delete();
    delete fPionEfficiencies;
    fPionEfficiencies = NULL;
  }
  if(fProtonEfficiencies){
    fProtonEfficiencies->Delete();
    delete fProtonEfficiencies;
    fProtonEfficiencies = NULL;
  }
  if(fThresholds){
    fThresholds->Delete();
    delete fThresholds;
    fThresholds = NULL;
  }
}
