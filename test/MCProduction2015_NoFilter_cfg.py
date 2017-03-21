import FWCore.ParameterSet.Config as cms
from Configuration.EventContent.EventContent_cff   import *

import CMS3.NtupleMaker.configProcessName as configProcessName
configProcessName.name="PAT"
configProcessName.name2="RECO"
configProcessName.isFastSim=False

relval = False
if relval: configProcessName.name="RECO"

# CMS3
process = cms.Process("CMS3")

# Version Control For Python Configuration Files
process.configurationMetadata = cms.untracked.PSet(
        version    = cms.untracked.string('$Revision: 1.11 $'),
        annotation = cms.untracked.string('CMS3'),
        name       = cms.untracked.string('CMS3 test configuration')
)

# load event level configurations
process.load('Configuration/EventContent/EventContent_cff')
process.load("Configuration.StandardSequences.Services_cff")
process.load('Configuration.Geometry.GeometryRecoDB_cff')
process.load("Configuration.StandardSequences.MagneticField_cff")
process.load("Configuration.StandardSequences.FrontierConditions_GlobalTag_condDBv2_cff")
process.load("Configuration.StandardSequences.GeometryRecoDB_cff")

# services
process.load("FWCore.MessageLogger.MessageLogger_cfi")
process.GlobalTag.globaltag = "80X_mcRun2_asymptotic_2016_miniAODv2_v0"
process.MessageLogger.cerr.FwkReport.reportEvery = 100
process.MessageLogger.cerr.threshold  = ''
process.MessageLogger.suppressWarning = cms.untracked.vstring('ecalLaserCorrFilter','manystripclus53X','toomanystripclus53X')
process.options = cms.untracked.PSet( allowUnscheduled = cms.untracked.bool(True),SkipEvent = cms.untracked.vstring('ProductNotFound') )

process.out = cms.OutputModule("PoolOutputModule",
                               fileName     = cms.untracked.string('ntuple.root'),
                               dropMetaData = cms.untracked.string("NONE"),
                               basketSize = cms.untracked.int32(16384*100)
)
process.out.outputCommands = cms.untracked.vstring( 'drop *' )
process.out.outputCommands.extend(cms.untracked.vstring('keep *_*Maker*_*_CMS3*'))
process.outpath = cms.EndPath(process.out)

#load cff and third party tools
from JetMETCorrections.Configuration.DefaultJEC_cff import *
from JetMETCorrections.Configuration.JetCorrectionServices_cff import *
from JetMETCorrections.Configuration.CorrectedJetProducersDefault_cff import *
from JetMETCorrections.Configuration.CorrectedJetProducers_cff import *
from JetMETCorrections.Configuration.CorrectedJetProducersAllAlgos_cff import *
process.load('JetMETCorrections.Configuration.DefaultJEC_cff')
# from RecoJets.JetProducers.fixedGridRhoProducerFastjet_cfi import *
# process.fixedGridRhoFastjetAll = fixedGridRhoFastjetAll.clone(pfCandidatesTag = 'packedPFCandidates')

#Electron Identification for PHYS 14
from PhysicsTools.SelectorUtils.tools.vid_id_tools import *  
from PhysicsTools.SelectorUtils.centralIDRegistry import central_id_registry
process.load("RecoEgamma.ElectronIdentification.egmGsfElectronIDs_cfi")
process.load("RecoEgamma.ElectronIdentification.ElectronMVAValueMapProducer_cfi")
process.egmGsfElectronIDs.physicsObjectSrc = cms.InputTag('slimmedElectrons',"",configProcessName.name)
process.electronMVAValueMapProducer.srcMiniAOD = cms.InputTag('slimmedElectrons',"",configProcessName.name)
process.egmGsfElectronIDSequence = cms.Sequence(process.electronMVAValueMapProducer * process.egmGsfElectronIDs)
my_id_modules = ['RecoEgamma.ElectronIdentification.Identification.cutBasedElectronID_Spring15_25ns_V1_cff',
                 'RecoEgamma.ElectronIdentification.Identification.heepElectronID_HEEPV60_cff',
                 'RecoEgamma.ElectronIdentification.Identification.mvaElectronID_Spring15_25ns_nonTrig_V1_cff',
                 'RecoEgamma.ElectronIdentification.Identification.mvaElectronID_Spring15_25ns_Trig_V1_cff',
                 'RecoEgamma.ElectronIdentification.Identification.mvaElectronID_Spring16_GeneralPurpose_V1_cff',
                 'RecoEgamma.ElectronIdentification.Identification.mvaElectronID_Spring16_HZZ_V1_cff']
for idmod in my_id_modules:
    setupAllVIDIdsInModule(process,idmod,setupVIDElectronSelection)

# Load Ntuple producer cff
process.load("CMS3.NtupleMaker.cms3CoreSequences_cff")
process.load("CMS3.NtupleMaker.cms3GENSequence_cff")
process.load("CMS3.NtupleMaker.cms3PFSequence_cff")
    
from PhysicsTools.PatAlgos.tools.jetTools import updateJetCollection
from PhysicsTools.PatAlgos.tools.jetTools import *
deep_discriminators = ["deepFlavourJetTags:probudsg", "deepFlavourJetTags:probb", "deepFlavourJetTags:probc", "deepFlavourJetTags:probbb", "deepFlavourJetTags:probcc" ]
updateJetCollection(
    process,
    jetSource = cms.InputTag('slimmedJets'),
   jetCorrections = ('AK4PFchs', cms.vstring([]), 'None'),
    btagDiscriminators = deep_discriminators
)
updateJetCollection(
    process,
    labelName = 'Puppi',
    jetSource = cms.InputTag('slimmedJetsPuppi'),
   jetCorrections = ('AK4PFchs', cms.vstring([]), 'None'),
    btagDiscriminators = deep_discriminators
)

# Needed for the above updateJetCollection() calls
process.pfJetMaker.pfJetsInputTag = cms.InputTag('selectedUpdatedPatJets')
process.pfJetPUPPIMaker.pfJetsInputTag = cms.InputTag('selectedUpdatedPatJetsPuppi')

# Hypothesis cuts
process.hypDilepMaker.TightLepton_PtCut  = cms.double(10.0)
process.hypDilepMaker.LooseLepton_PtCut  = cms.double(10.0)

#Options for Input
process.source = cms.Source("PoolSource",
                            fileNames = cms.untracked.vstring(
                                # 'file:/hadoop/cms/phedex/store/mc/RunIISpring15MiniAODv2/DYJetsToLL_M-50_TuneCUETP8M1_13TeV-amcatnloFXFX-pythia8/MINIAODSIM/74X_mcRun2_asymptotic_v2-v1/60000/7AEAFCAD-266F-E511-8A2A-001E67A3F3DF.root',
                                # 'root://cmsxrootd.fnal.gov//store/mc/RunIIFall15MiniAODv1/WWTo2L2Nu_13TeV-powheg/MINIAODSIM/PU25nsData2015v1_76X_mcRun2_asymptotic_v12-v1/50000/0E47EC63-7B9D-E511-B714-B083FED426E5.root
#         'file:/hadoop/cms/phedex/store/mc/RunIISpring16MiniAODv1/ttbb_4FS_ckm_amcatnlo_madspin_pythia8/MINIAODSIM/PUSpring16_80X_mcRun2_asymptotic_2016_v3-v1/60000/F4EA8D09-9002-E611-9D1B-1CC1DE19274E.root',
#                                '/store/mc/RunIISpring16MiniAODv2/WZTo3LNu_TuneCUETP8M1_13TeV-powheg-pythia8/MINIAODSIM/PUSpring16_80X_mcRun2_asymptotic_2016_miniAODv2_v0-v1/60000/D63C4E53-D91B-E611-AC83-FA163E5810F7.root',
                                # 'file:RelValProdQCD_Pt_3000_3500_13.root'
                                'file:/home/users/namin/2017/slimming/CMSSW_8_0_26_patch1/src/CMS3/NtupleMaker/test/A8B84A69-C1D7-E611-831F-5065F382B2D1.root'
                            )
)
process.source.noEventSort = cms.untracked.bool( True )

#Max Events
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(3000) )


#Run corrected MET maker

#configurable options =======================================================================
runOnData=False #data/MC switch
usePrivateSQlite=False #use external JECs (sqlite file)
applyResiduals=False #application of residual corrections. Have to be set to True once the 13 TeV residual corrections are available. False to be kept meanwhile. Can be kept to False later for private tests or for analysis checks and developments (not the official recommendation!).
#===================================================================

if usePrivateSQlite:
    from CondCore.DBCommon.CondDBSetup_cfi import *
    import os
    era="Summer15_25nsV5_MC"
    process.jec = cms.ESSource("PoolDBESSource",CondDBSetup,
                               connect = cms.string( "sqlite_file:"+era+".db" ),
                               toGet =  cms.VPSet(
            cms.PSet(
                record = cms.string("JetCorrectionsRecord"),
                tag = cms.string("JetCorrectorParametersCollection_"+era+"_AK4PF"),
                label= cms.untracked.string("AK4PF")
                ),
            cms.PSet(
                record = cms.string("JetCorrectionsRecord"),
                tag = cms.string("JetCorrectorParametersCollection_"+era+"_AK4PFchs"),
                label= cms.untracked.string("AK4PFchs")
                ),
            )
                               )
    process.es_prefer_jec = cms.ESPrefer("PoolDBESSource",'jec')

### =================================================================================
#jets are rebuilt from those candidates by the tools, no need to do anything else
### =================================================================================

from PhysicsTools.PatUtils.tools.runMETCorrectionsAndUncertainties import runMetCorAndUncFromMiniAOD


#default configuration for miniAOD reprocessing, change the isData flag to run on data
#for a full met computation, remove the pfCandColl input
runMetCorAndUncFromMiniAOD(process,
                           isData=runOnData,
                           )

### -------------------------------------------------------------------
### the lines below remove the L2L3 residual corrections when processing data
### -------------------------------------------------------------------
if not applyResiduals:
    process.patPFMetT1T2Corr.jetCorrLabelRes = cms.InputTag("L3Absolute")
    process.patPFMetT1T2SmearCorr.jetCorrLabelRes = cms.InputTag("L3Absolute")
    process.patPFMetT2Corr.jetCorrLabelRes = cms.InputTag("L3Absolute")
    process.patPFMetT2SmearCorr.jetCorrLabelRes = cms.InputTag("L3Absolute")
    process.shiftedPatJetEnDown.jetCorrLabelUpToL3Res = cms.InputTag("ak4PFCHSL1FastL2L3Corrector")
    process.shiftedPatJetEnUp.jetCorrLabelUpToL3Res = cms.InputTag("ak4PFCHSL1FastL2L3Corrector")
### ------------------------------------------------------------------

# end Run corrected MET maker

# process.p = cms.Path( 
#   process.metFilterMaker *
#   process.egmGsfElectronIDSequence *     
#   process.vertexMaker *
#   process.secondaryVertexMaker *
#   process.eventMaker *
#   process.pfCandidateMaker *
#   process.isoTrackMaker *
#   process.electronMaker *
#   process.muonMaker *
#   process.pfJetMaker *
#   process.pfJetPUPPIMaker *
#   process.subJetMaker *
#   process.pfmetMaker *
#   process.pfmetpuppiMaker *
#   process.hltMakerSequence *
#   process.pftauMaker *
#   process.photonMaker *
#   process.genMaker *
#   process.genJetMaker *
#   process.candToGenAssMaker * # requires electronMaker, muonMaker, pfJetMaker, photonMaker
#   process.pdfinfoMaker *
#   process.puSummaryInfoMaker *
#   process.hypDilepMaker
# )

process.MessageLogger.cerr.FwkReport.reportEvery = 100
process.eventMaker.isData                        = cms.bool(False)

process.Timing = cms.Service("Timing")
