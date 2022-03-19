add_llvm_component_library(LLVMPatmosSinglePath
  PatmosSinglePathInfo.cpp
  PatmosSPClone.cpp
  PatmosSPMark.cpp
  PatmosSPPrepare.cpp
  PatmosSPBundling.cpp
  DataCacheAccessElimination.cpp
  PatmosSPReduce.cpp
  RAInfo.cpp
  SPScope.cpp
  SPScheduler.cpp
  PredicateDefinition.cpp
  InstructionLevelCET.cpp
  MemoryAccessAnalysis.cpp
  
  LINK_COMPONENTS
  PatmosInfo 
  MC 
  Support
 
  ADD_TO_COMPONENT
  Patmos
  
  )