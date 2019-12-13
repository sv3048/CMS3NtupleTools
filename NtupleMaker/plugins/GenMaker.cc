#include "CMS3/NtupleMaker/interface/plugins/GenMaker.h"
#include <ZZMatrixElement/MELA/interface/PDGHelpers.h>
#include "MELAStreamHelpers.hh"


typedef math::XYZTLorentzVectorF LorentzVector;

using namespace std;
using namespace edm;
using namespace reco;


GenMaker::GenMaker(const edm::ParameterSet& iConfig) :
  aliasprefix_(iConfig.getUntrackedParameter<string>("aliasprefix")),
  year(iConfig.getParameter<int>("year")),

  xsecOverride(static_cast<float>(iConfig.getParameter<double>("xsec"))),
  brOverride(static_cast<float>(iConfig.getParameter<double>("BR"))),

  LHEInputTag_(iConfig.getParameter<edm::InputTag>("LHEInputTag")),
  genEvtInfoInputTag_(iConfig.getParameter<edm::InputTag>("genEvtInfoInputTag")),
  prunedGenParticlesInputTag_(iConfig.getParameter<edm::InputTag>("prunedGenParticlesInputTag")),
  packedGenParticlesInputTag_(iConfig.getParameter<edm::InputTag>("packedGenParticlesInputTag")),
  genMETInputTag_(iConfig.getParameter<edm::InputTag>("genMETInputTag")),

  ntuplePackedGenParticles_(iConfig.getParameter<bool>("ntuplePackedGenParticles")),

  superMH(static_cast<float>(iConfig.getParameter<double>("superMH"))),

  doHiggsKinematics(iConfig.getParameter<bool>("doHiggsKinematics")),
  candVVmode(MELAEvent::getCandidateVVModeFromString(iConfig.getUntrackedParameter<string>("candVVmode"))),
  decayVVmode(iConfig.getParameter<int>("decayVVmode")),
  lheMElist(iConfig.getParameter< std::vector<std::string> >("lheMElist"))
{
  consumesMany<LHEEventProduct>();
  LHERunInfoToken = consumes<LHERunInfoProduct, edm::InRun>(LHEInputTag_);
  genEvtInfoToken = consumes<GenEventInfoProduct>(genEvtInfoInputTag_);
  prunedGenParticlesToken = consumes<reco::GenParticleCollection>(prunedGenParticlesInputTag_);
  packedGenParticlesToken = consumes<pat::PackedGenParticleCollection>(packedGenParticlesInputTag_);
  genMETToken = consumes< edm::View<pat::MET> >(genMETInputTag_);

  consumesMany<HepMCProduct>();

  if (year==2016) LHEHandler::set_maxlines_print_header(1000);
  else LHEHandler::set_maxlines_print_header(-1);
  lheHandler_default = std::make_shared<LHEHandler>(
    candVVmode, decayVVmode,
    ((candVVmode!=MELAEvent::nCandidateVVModes && (!lheMElist.empty() || doHiggsKinematics)) ? LHEHandler::doHiggsKinematics : LHEHandler::noKinematics),
    year, LHEHandler::keepDefaultPDF, LHEHandler::keepDefaultQCDOrder
    );
  lheHandler_NNPDF30_NLO = std::make_shared<LHEHandler>(
    MELAEvent::nCandidateVVModes, -1,
    LHEHandler::noKinematics,
    year, LHEHandler::tryNNPDF30, LHEHandler::tryNLO
    );

  // Setup ME computation
  setupMELA();

  produces<GenInfo>();
}

GenMaker::~GenMaker(){
  // Clear ME computations
  cleanMELA();
}

void GenMaker::beginJob(){}
void GenMaker::endJob(){}

void GenMaker::beginRun(const edm::Run& iRun, const edm::EventSetup& /*iSetup*/){
  static bool firstRun = true;
  if (firstRun){ // Do these only at the first run
    // Extract LHE header
    edm::Handle<LHERunInfoProduct> lhe_runinfo;
    iRun.getByLabel(LHEInputTag_, lhe_runinfo);
    lheHandler_default->setHeaderFromRunInfo(&lhe_runinfo);
    lheHandler_NNPDF30_NLO->setHeaderFromRunInfo(&lhe_runinfo);
    firstRun=false;
  }
}

void GenMaker::produce(edm::Event& iEvent, const edm::EventSetup& iSetup){
  auto result = std::make_unique<GenInfo>();

  edm::Handle<reco::GenParticleCollection> prunedGenParticlesHandle;
  iEvent.getByToken(prunedGenParticlesToken, prunedGenParticlesHandle);
  if (!prunedGenParticlesHandle.isValid()){
    edm::LogError("GenMaker") << "GenMaker::produce: Failed to retrieve pruned gen. particle collection...";
    return;
  }
  std::vector<reco::GenParticle> const* prunedGenParticles = prunedGenParticlesHandle.product();

  /*
  edm::Handle<pat::PackedGenParticleCollection> packedGenParticlesHandle;
  iEvent.getByToken(packedGenParticlesToken, packedGenParticlesHandle);
  if (!packedGenParticlesHandle.isValid()){
    edm::LogError("GenMaker") << "GenMaker::produce: Failed to retrieve packed gen. particle collection...";
    return;
  }
  std::vector<pat::PackedGenParticle> const* packedGenParticles = packedGenParticlesHandle.product();
  */

  // Gen. MET
  edm::Handle< edm::View<pat::MET> > genMETHandle;
  iEvent.getByToken(genMETToken, genMETHandle);
  if (!genMETHandle.isValid()) throw cms::Exception("GenMaker::produce: Error getting the gen. MET handle...");
  result->genmet_met = (genMETHandle->front()).genMET()->pt();
  result->genmet_metPhi = (genMETHandle->front()).genMET()->phi();

  // Gen. and LHE weights
  edm::Handle<GenEventInfoProduct> genEvtInfoHandle;
  iEvent.getByToken(genEvtInfoToken, genEvtInfoHandle);
  if (!genEvtInfoHandle.isValid()){
    edm::LogError("GenMaker") << "GenMaker::produce: Failed to retrieve gen. event info...";
    return;
  }
  else{
    result->qscale = genEvtInfoHandle->qScale();
    result->alphaS = genEvtInfoHandle->alphaQCD();
  }

  if (xsecOverride>0.f && brOverride>0.f) result->xsec = xsecOverride*brOverride;
  else if (xsecOverride>0.f) result->xsec = xsecOverride;

  std::vector< edm::Handle<HepMCProduct> > HepMCProductHandles;
  iEvent.getManyByType(HepMCProductHandles);

  std::vector< edm::Handle<LHEEventProduct> > LHEEventInfoList;
  iEvent.getManyByType(LHEEventInfoList);

  float genps_weight = 0;
  if (!HepMCProductHandles.empty()){
    const HepMC::GenEvent* HepMCGenEvent = HepMCProductHandles.front()->GetEvent();

    result->pThat = HepMCGenEvent->event_scale();

    auto HepMCweights = HepMCGenEvent->weights();
    if (!HepMCweights.empty()) genps_weight = HepMCweights.front();
  }

  if (!LHEEventInfoList.empty() && LHEEventInfoList.front().isValid()){
    edm::Handle<LHEEventProduct>& LHEEventInfo = LHEEventInfoList.front();

    lheHandler_default->setHandle(&LHEEventInfo);
    lheHandler_default->extract();
    // Special case for the default:
    // Compute MEs
    if (!lheMElist.empty()){
      MELACandidate* cand = lheHandler_default->getBestCandidate();
      doMELA(cand, *result);
    }
    // Record the LHE-level particles (filled if lheHandler_default->doKinematics>=LHEHandler::doBasicKinematics)
    std::vector<MELAParticle*> const& basicParticleList = lheHandler_default->getParticleList();
    for (auto it_part = basicParticleList.cbegin(); it_part != basicParticleList.cend(); it_part++){
      MELAParticle* part_i = *it_part;
      result->lheparticles_px.push_back(part_i->x());
      result->lheparticles_py.push_back(part_i->y());
      result->lheparticles_pz.push_back(part_i->z());
      result->lheparticles_E.push_back(part_i->t());
      result->lheparticles_id.push_back(part_i->id);
      result->lheparticles_status.push_back(part_i->genStatus);

      int lheparticles_mother0_index=-1; int lheparticles_mother1_index=-1;
      for (int imom=0; imom<std::min(part_i->getNMothers(), 2); imom++){
        MELAParticle* mother = part_i->getMother(imom);
        int& lheparticles_mother_index = (imom==0 ? lheparticles_mother0_index : lheparticles_mother1_index);
        for (auto jt_part = basicParticleList.cbegin(); jt_part != basicParticleList.cend(); jt_part++){
          MELAParticle* part_j = *jt_part;
          if (part_j==mother){
            lheparticles_mother_index = (jt_part - basicParticleList.cbegin());
            break;
          }
        }
      }
      result->lheparticles_mother0_index.push_back(lheparticles_mother0_index);
      result->lheparticles_mother1_index.push_back(lheparticles_mother1_index);
    }


    lheHandler_NNPDF30_NLO->setHandle(&LHEEventInfo);
    lheHandler_NNPDF30_NLO->extract();

    if (genEvtInfoHandle.isValid()){
      result->genHEPMCweight_default = genEvtInfoHandle->weight();
      if (result->genHEPMCweight_default==1.) result->genHEPMCweight_default = lheHandler_default->getLHEOriginalWeight();
    }
    else result->genHEPMCweight_default = lheHandler_default->getLHEOriginalWeight(); // Default was also 1, so if !genEvtInfoHandle.isValid(), the statement still passes
    result->genHEPMCweight_NNPDF30 = result->genHEPMCweight_default; // lheHandler_NNPDF30_NLO->getLHEOriginalWeight() should give the same value
    result->genHEPMCweight_default *= lheHandler_default->getWeightRescale();
    result->genHEPMCweight_NNPDF30 *= lheHandler_NNPDF30_NLO->getWeightRescale();

    result->LHEweight_QCDscale_muR1_muF1 = lheHandler_default->getLHEWeight(0, 1.);
    result->LHEweight_QCDscale_muR1_muF2 = lheHandler_default->getLHEWeight(1, 1.);
    result->LHEweight_QCDscale_muR1_muF0p5 = lheHandler_default->getLHEWeight(2, 1.);
    result->LHEweight_QCDscale_muR2_muF1 = lheHandler_default->getLHEWeight(3, 1.);
    result->LHEweight_QCDscale_muR2_muF2 = lheHandler_default->getLHEWeight(4, 1.);
    result->LHEweight_QCDscale_muR2_muF0p5 = lheHandler_default->getLHEWeight(5, 1.);
    result->LHEweight_QCDscale_muR0p5_muF1 = lheHandler_default->getLHEWeight(6, 1.);
    result->LHEweight_QCDscale_muR0p5_muF2 = lheHandler_default->getLHEWeight(7, 1.);
    result->LHEweight_QCDscale_muR0p5_muF0p5 = lheHandler_default->getLHEWeight(8, 1.);

    result->LHEweight_PDFVariation_Up_default = lheHandler_default->getLHEWeight_PDFVariationUpDn(1, 1.);
    result->LHEweight_PDFVariation_Dn_default = lheHandler_default->getLHEWeight_PDFVariationUpDn(-1, 1.);
    result->LHEweight_AsMZ_Up_default = lheHandler_default->getLHEWeigh_AsMZUpDn(1, 1.);
    result->LHEweight_AsMZ_Dn_default = lheHandler_default->getLHEWeigh_AsMZUpDn(-1, 1.);

    result->LHEweight_PDFVariation_Up_NNPDF30 = lheHandler_NNPDF30_NLO->getLHEWeight_PDFVariationUpDn(1, 1.);
    result->LHEweight_PDFVariation_Dn_NNPDF30 = lheHandler_NNPDF30_NLO->getLHEWeight_PDFVariationUpDn(-1, 1.);
    result->LHEweight_AsMZ_Up_NNPDF30 = lheHandler_NNPDF30_NLO->getLHEWeigh_AsMZUpDn(1, 1.);
    result->LHEweight_AsMZ_Dn_NNPDF30 = lheHandler_NNPDF30_NLO->getLHEWeigh_AsMZUpDn(-1, 1.);

    result->xsec_lhe = LHEEventInfo->originalXWGTUP();

    lheHandler_default->clear();
    lheHandler_NNPDF30_NLO->clear();
  }
  else{
    if (genEvtInfoHandle.isValid()) result->genHEPMCweight_default = genEvtInfoHandle->weight();
    else result->genHEPMCweight_default = genps_weight;
    result->genHEPMCweight_NNPDF30 = result->genHEPMCweight_default; // No other choice really
  }

  // Extract PS weights
  {
    const auto& genweights = genEvtInfoHandle->weights();
    if (genweights.size() > 1){
      if ((genweights.size() != 14 && genweights.size() != 46) || genweights.at(0) != genweights.at(1)){
        cms::Exception e("GenWeights");
        e << "Expected to find 1 gen weight, or 14 or 46 with the first two the same, found " << genweights.size() << ":\n";
        for (auto w : genweights) e << w << " ";
        throw e;
      }
      auto const& nominal = genweights[0];
      result->PythiaWeight_isr_muRoneoversqrt2 = genweights[2] / nominal;
      result->PythiaWeight_fsr_muRoneoversqrt2 = genweights[3] / nominal;
      result->PythiaWeight_isr_muRsqrt2 = genweights[4] / nominal;
      result->PythiaWeight_fsr_muRsqrt2 = genweights[5] / nominal;

      result->PythiaWeight_isr_muR0p5 = genweights[6] / nominal;
      result->PythiaWeight_fsr_muR0p5 = genweights[7] / nominal;
      result->PythiaWeight_isr_muR2 = genweights[8] / nominal;
      result->PythiaWeight_fsr_muR2 = genweights[9] / nominal;

      result->PythiaWeight_isr_muR0p25 = genweights[10] / nominal;
      result->PythiaWeight_fsr_muR0p25 = genweights[11] / nominal;
      result->PythiaWeight_isr_muR4 = genweights[12] / nominal;
      result->PythiaWeight_fsr_muR4 = genweights[13] / nominal;
    }
  }

  {
    float& sumEt = result->sumEt; sumEt=0;
    LorentzVector tempvect(0, 0, 0, 0);
    for (std::vector<reco::GenParticle>::const_iterator genps_it = prunedGenParticles->begin(); genps_it != prunedGenParticles->end(); genps_it++) {
      int id = genps_it->pdgId();
      if (PDGHelpers::isANeutrino(id) && genps_it->status()==1)
        tempvect += LorentzVector(
          genps_it->p4().x(),
          genps_it->p4().y(),
          genps_it->p4().z(),
          genps_it->p4().e()
        );
    }
    sumEt = tempvect.pt();
  }

  iEvent.put(std::move(result));
}


/******************/
/* ME COMPUTATION */
/******************/
void GenMaker::setupMELA(){
  if (candVVmode==MELAEvent::nCandidateVVModes || lheMElist.empty()){
    this->usesResource("GenMakerNoMELA");
    return;
  }
  this->usesResource("MELA");

  using namespace CMS3MELAHelpers;

  setupMela(year, superMH, TVar::ERROR); // Sets up MELA only once

  lheMEblock.buildMELABranches(lheMElist, true);
}
void GenMaker::doMELA(MELACandidate* cand, GenInfo& genInfo){
  using namespace CMS3MELAHelpers;
  if (melaHandle && cand){
    melaHandle->setCurrentCandidate(cand);

    lheMEblock.computeMELABranches();
    lheMEblock.pushMELABranches();
    lheMEblock.getBranchValues(genInfo.LHE_ME_weights); // Record the MEs into the EDProducer product

    melaHandle->resetInputEvent();
  }
}
void GenMaker::cleanMELA(){
  using namespace CMS3MELAHelpers;
  // Shared pointer should be able to clear itself
  //clearMela();
}


//define this as a plug-in
DEFINE_FWK_MODULE(GenMaker);
