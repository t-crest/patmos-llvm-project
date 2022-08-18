set(LLVM_LINK_COMPONENTS
  PatmosSinglePath
  )

set(PatmosSource
  PredicatedBlockTest.cpp
  MemoryAccessAnalysisTest.cpp
  BoundedDominatorAnalysisTest.cpp
  ListSchedulerTest.cpp
  )

add_llvm_unittest(SinglePathTests
  ${PatmosSource}
  )
