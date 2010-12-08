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
// Class AliHFEtpcPIDqa
// Monitoring TPC PID in the HFE PID montioring framework. The following
// quantities are monitored:
//   TPC dE/dx (Number of sigmas)
//   TPC dE/dx (Absolute values)
// (Always as function of momentum, particle species and centrality 
// before and after cut)
// More information about the PID monitoring framework can be found in
// AliHFEpidQAmanager.cxx and AliHFEdetPIDqa.cxx
//
// Author:
//    Markus Fasel <M.Fasel@gsi.de>
//
#include <TClass.h>
#include <TH2.h>
#include <THnSparse.h>
#include <TString.h>

#include "AliAODTrack.h"
#include "AliESDtrack.h"
#include "AliESDpid.h"
#include "AliLog.h"
#include "AliPID.h"

#include "AliHFEcollection.h"
#include "AliHFEpidBase.h"
#include "AliHFEtools.h"
#include "AliHFEtpcPIDqa.h"

//_________________________________________________________
AliHFEtpcPIDqa::AliHFEtpcPIDqa():
    AliHFEdetPIDqa()
  , fHistos(NULL)
{
  //
  // Dummy constructor
  //
}

//_________________________________________________________
AliHFEtpcPIDqa::AliHFEtpcPIDqa(const char* name):
    AliHFEdetPIDqa(name, "QA for TPC")
  , fHistos(NULL)
{
  //
  // Default constructor
  //
}

//_________________________________________________________
AliHFEtpcPIDqa::AliHFEtpcPIDqa(const AliHFEtpcPIDqa &o):
    AliHFEdetPIDqa(o)
  , fHistos()
{
  //
  // Copy constructor
  //
  o.Copy(*this);
}

//_________________________________________________________
AliHFEtpcPIDqa &AliHFEtpcPIDqa::operator=(const AliHFEtpcPIDqa &o){
  //
  // Do assignment
  //
  AliHFEdetPIDqa::operator=(o);
  if(&o != this) o.Copy(*this);
  return *this;
}

//_________________________________________________________
AliHFEtpcPIDqa::~AliHFEtpcPIDqa(){
  //
  // Destructor
  //
  if(fHistos) delete fHistos;
}

//_________________________________________________________
void AliHFEtpcPIDqa::Copy(TObject &o) const {
  //
  // Make copy
  //
  AliHFEtpcPIDqa &target = dynamic_cast<AliHFEtpcPIDqa &>(o);
  if(target.fHistos){
    delete target.fHistos;
    target.fHistos = NULL;
  }
  if(fHistos) target.fHistos = new AliHFEcollection(*fHistos);
}

//_________________________________________________________
Long64_t AliHFEtpcPIDqa::Merge(TCollection *coll){
  //
  // Merge with other objects
  //
  if(!coll) return 0;
  if(coll->IsEmpty()) return 1;

  TIter it(coll);
  AliHFEtpcPIDqa *refQA = NULL;
  TObject *o = NULL;
  Long64_t count = 0;
  TList listHistos;
  while((o = it())){
    refQA = dynamic_cast<AliHFEtpcPIDqa *>(o);
    if(!refQA) continue;

    listHistos.Add(refQA->fHistos);
    count++; 
  }
  fHistos->Merge(&listHistos);
  return count + 1;
}

//_________________________________________________________
void AliHFEtpcPIDqa::Initialize(){
  //
  // Define Histograms
  //

  fHistos = new AliHFEcollection("tpcqahistos", "Collection of TPC QA histograms");

  // Make common binning
  const Int_t kNdim = 5;
  const Int_t kPIDbins = AliPID::kSPECIES + 1;
  const Int_t kPbins = 1000;
  const Int_t kSteps = 2;
  const Int_t kCentralityBins = 20;
  const Double_t kMinPID = -1;
  const Double_t kMinP = 0.;
  const Double_t kMaxPID = (Double_t)AliPID::kSPECIES;
  const Double_t kMaxP = 20.;
  
  // 1st histogram: TPC dEdx: (species, p, dEdx, step)
  const Int_t kDedxbins = 600;
  Int_t nBinsdEdx[kNdim] = {kPIDbins, kPbins, kDedxbins, kSteps, kCentralityBins};
  Double_t mindEdx[kNdim] =  {kMinPID, kMinP, 0., 0., 0.};
  Double_t maxdEdx[kNdim] =  {kMaxPID, kMaxP, 300, 2., 100.}; 
  fHistos->CreateTHnSparse("tpcDedx", "TPC signal; species; p [GeV/c]; TPC signal [a.u.]; Centrality; Selection Step", kNdim, nBinsdEdx, mindEdx, maxdEdx);
  // 2nd histogram: TPC sigmas: (species, p nsigma, step)
  const Int_t kSigmaBins = 1400;
  Int_t nBinsSigma[kNdim] = {kPIDbins, kPbins, kSigmaBins, kSteps, kCentralityBins};
  Double_t minSigma[kNdim] = {kMinPID, kMinP, -12., 0., 0.};
  Double_t maxSigma[kNdim] = {kMaxPID, kMaxP, 12., 2., 100.};
  fHistos->CreateTHnSparse("tpcnSigma", "TPC signal; species; p [GeV/c]; TPC signal [a.u.]; Centrality; Selection Step", kNdim, nBinsSigma, minSigma, maxSigma);

  // General TPC QA
}

//_________________________________________________________
void AliHFEtpcPIDqa::ProcessTrack(AliHFEpidObject *track, AliHFEdetPIDqa::EStep_t step){
  //
  // Fill TPC histograms
  //
  AliDebug(1, Form("QA started for TPC PID for step %d", (Int_t)step));
  Int_t species = track->GetAbInitioPID();
  Float_t centrality = track->GetCentrality();
  const AliVParticle *rectrack = track->GetRecTrack();
  if(species >= AliPID::kSPECIES) species = -1;
  if(!TString(rectrack->IsA()->GetName()).CompareTo("AliESDtrack")) ProcessESDtrack(dynamic_cast<const AliESDtrack *>(rectrack), step, species, centrality);
  else if(!TString(rectrack->IsA()->GetName()).CompareTo("AliAODTrack")) ProcessAODtrack(dynamic_cast<const AliAODTrack *>(rectrack), step, species, centrality);
  else  AliWarning(Form("Object type %s not supported\n", rectrack->IsA()->GetName()));
}

//_________________________________________________________
void AliHFEtpcPIDqa::ProcessESDtrack(const AliESDtrack *track, AliHFEdetPIDqa::EStep_t step, Int_t species, Float_t centrality){
  //
  // Process track as ESD track
  //
  if(!fESDpid){
    AliError("No ESD PID object available");
    return;
  }
  AliDebug(1, Form("Monitoring particle of type %d for step %d", species, step));
  
  Double_t contentSignal[5];
  contentSignal[0] = species;
  contentSignal[1] = track->GetInnerParam() ? track->GetInnerParam()->P() : track->P();
  contentSignal[2] = track->GetTPCsignal();
  contentSignal[3] = step;
  contentSignal[4] = centrality;
  (dynamic_cast<THnSparseF *>(fHistos->Get("tpcDedx")))->Fill(contentSignal);

  contentSignal[2] = fESDpid->NumberOfSigmasTPC(track, AliPID::kElectron); 
  (dynamic_cast<THnSparseF *>(fHistos->Get("tpcnSigma")))->Fill(contentSignal);
}

//_________________________________________________________
void AliHFEtpcPIDqa::ProcessAODtrack(const AliAODTrack * /*track*/, AliHFEdetPIDqa::EStep_t /*step*/, Int_t /*species*/, Float_t /*centrality*/){
  //
  // Process track as AOD track
  //
  AliInfo("Method implemented soon!");
}

//_________________________________________________________
TH2 *AliHFEtpcPIDqa::MakeSpectrumdEdx(AliHFEdetPIDqa::EStep_t istep, Int_t species){
  //
  // Plot the Spectrum
  //
  THnSparseF *hSignal = dynamic_cast<THnSparseF *>(fHistos->Get("tpcDedx"));
  hSignal->GetAxis(3)->SetRange(istep + 1, istep + 1);
  if(species > 0 && species < AliPID::kSPECIES)
    hSignal->GetAxis(0)->SetRange(2 + species, 2 + species);
  TH2 *hTmp = hSignal->Projection(2,1);
  Char_t hname[256], htitle[256];
  sprintf(hname, "hTPCsignal%s", istep == AliHFEdetPIDqa::kBeforePID ? "before" : "after");
  sprintf(htitle, "TPC dE/dx Spectrum %s selection", istep == AliHFEdetPIDqa::kBeforePID ? "before" : "after");
  if(species > -1){
    strncat(hname, AliPID::ParticleName(species), strlen(AliPID::ParticleName(species)));
     Char_t speciesname[256];
     sprintf(speciesname, " for %ss", AliPID::ParticleName(species));
     strncat(htitle, speciesname, strlen(speciesname));
  }
  hTmp->SetName(hname);
  hTmp->SetTitle(htitle);
  hTmp->SetStats(kFALSE);
  hTmp->GetXaxis()->SetTitle("p [GeV/c]");
  hTmp->GetYaxis()->SetTitle("TPC signal [a.u.]");
  hSignal->GetAxis(3)->SetRange(0, hSignal->GetAxis(3)->GetNbins());
  hSignal->GetAxis(0)->SetRange(0, hSignal->GetAxis(0)->GetNbins());
  return hTmp;
}

//_________________________________________________________
TH2 *AliHFEtpcPIDqa::MakeSpectrumNSigma(AliHFEdetPIDqa::EStep_t istep, Int_t species){
  //
  // Plot the Spectrum
  //
  THnSparseF *hSignal = dynamic_cast<THnSparseF *>(fHistos->Get("tpcnSigma"));
  hSignal->GetAxis(3)->SetRange(istep + 1, istep + 1);
  if(species >= 0 && species < AliPID::kSPECIES)
    hSignal->GetAxis(0)->SetRange(2 + species, 2 + species);
  TH2 *hTmp = hSignal->Projection(2,1);
  Char_t hname[256], htitle[256];
  sprintf(hname, "hTPCsigma%s", istep == AliHFEdetPIDqa::kBeforePID ? "before" : "after");
  sprintf(htitle, "TPC dE/dx Spectrum[#sigma] %s selection", istep == AliHFEdetPIDqa::kBeforePID ? "before" : "after");
  if(species > -1){
    strncat(hname, AliPID::ParticleName(species), strlen(AliPID::ParticleName(species)));
     Char_t speciesname[256];
     sprintf(speciesname, " for %ss", AliPID::ParticleName(species));
     strncat(htitle, speciesname, strlen(speciesname));
  }
  hTmp->SetName(hname);
  hTmp->SetTitle(htitle);
  hTmp->SetStats(kFALSE);
  hTmp->GetXaxis()->SetTitle("p [GeV/c]");
  hTmp->GetYaxis()->SetTitle("TPC dE/dx - <dE/dx>|_{el} [#sigma]");
  hSignal->GetAxis(3)->SetRange(0, hSignal->GetAxis(3)->GetNbins());
  hSignal->GetAxis(0)->SetRange(0, hSignal->GetAxis(0)->GetNbins());
  return hTmp;
}

