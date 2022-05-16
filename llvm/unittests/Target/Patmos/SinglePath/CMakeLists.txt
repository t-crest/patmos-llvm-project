set(LLVM_LINK_COMPONENTS
  PatmosSinglePath
  )

set(PatmosSource
  PredicatedBlockTest.cpp
  MemoryAccessAnalysisTest.cpp
  BoundedDominatorAnalysisTest.cpp
  )

add_llvm_unittest(SinglePathTests
  ${PatmosSource}
  )
