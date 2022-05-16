#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "SinglePath/BoundedDominatorAnalysis.h"
#include "Mocks.h"

using namespace llvm;

using ::testing::Return;
using ::testing::Contains;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;

namespace {

typedef MockMBB<bool> MockMBB;
typedef MockLoop<bool> MockLoop;
typedef MockLoopInfo<bool> MockLoopInfo;

#define mockMBB(name, constant) mockMBBBase(name, constant)

bool constantBounds(const MockMBB *mbb){
  return mbb->payload;
}

TEST(boundedDominatorsAnalysisTest, OneMBB){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  set_preds_succs(mockMBB1, arr({}),arr({}));

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
}

TEST(boundedDominatorsAnalysisTest, Consecutive){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({}));

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
}

TEST(boundedDominatorsAnalysisTest, Branch){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2, &mockMBB3}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB2, &mockMBB3}), arr({}));

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4
    })
  );
}

TEST(boundedDominatorsAnalysisTest, Branch2){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  mockMBB(mockMBB6,false);

  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB5}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}), arr({&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB3, &mockMBB4}), arr({&mockMBB6}));
  set_preds_succs(mockMBB6, arr({&mockMBB5}), arr({}));

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB6],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5, &mockMBB6
    })
  );
}

TEST(boundedDominatorsAnalysisTest, Branch3){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2, &mockMBB3, &mockMBB4}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB5}));
  set_preds_succs(mockMBB4, arr({&mockMBB1}),arr({&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB2, &mockMBB3, &mockMBB4}), arr({}));

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopOneMBBVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB3
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopOneMBBConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopTwoMBBVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopTwoMBBConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopTwoLatchesConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3,&mockMBB4 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopTwoLatchesVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3,&mockMBB4 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopNonHeadExitConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopNonHeadExitVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4
    })
  );
}

TEST(boundedDominatorsAnalysisTest, NestedLoopConstantConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,true);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, NestedLoopConstantVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, NestedLoopVariableVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, NestedLoopVariableConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,true);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, TwoNonLatchHeaderPredsConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,true);
  mockMBB(mockMBB5,false);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB2,&mockMBB3}),arr({&mockMBB4,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB4}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, TwoNonLatchHeaderPredsVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB2,&mockMBB3}),arr({&mockMBB4,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB4}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopExitFromNestedConstantConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,true);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB2,&mockMBB4}), arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3 },
      { &mockMBB2, &mockMBB4 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3, &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopExitFromNestedConstantVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB2,&mockMBB4}), arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3 },
      { &mockMBB2, &mockMBB4 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3, &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopLatchFromNestedConstantConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,true);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3,&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

  MockLoop mockLoop1(nullptr,
      {&mockMBB2,&mockMBB3,&mockMBB4},
      {&mockMBB3,&mockMBB4},
      {&mockMBB2}
  );
  MockLoop mockLoop2(&mockLoop1,
      {&mockMBB3,&mockMBB4},
      {&mockMBB4},
      {&mockMBB3,&mockMBB4}
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopLatchFromNestedConstantVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3,&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

  MockLoop mockLoop1(nullptr,
      {&mockMBB2,&mockMBB3,&mockMBB4},
      {&mockMBB3,&mockMBB4},
      {&mockMBB2}
  );
  MockLoop mockLoop2(&mockLoop1,
      {&mockMBB3,&mockMBB4},
      {&mockMBB4},
      {&mockMBB3,&mockMBB4}
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(boundedDominatorsAnalysisTest, TwoConsecutiveLoopsConstantConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,true);
  mockMBB(mockMBB4,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(nullptr,
      { &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
}

TEST(boundedDominatorsAnalysisTest, TwoConsecutiveLoopsConstantVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(nullptr,
      { &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB4
    })
  );
}

TEST(boundedDominatorsAnalysisTest, TwoConsecutiveLoopsVariableConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,true);
  mockMBB(mockMBB4,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(nullptr,
      { &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB3, &mockMBB4
    })
  );
}

TEST(boundedDominatorsAnalysisTest, TwoConsecutiveLoopsVariableVariable){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(nullptr,
      { &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4
    })
  );
}

TEST(boundedDominatorsAnalysisTest, TwoConsecutiveLoopsBranchedConstConst){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  mockMBB(mockMBB6,true);
  mockMBB(mockMBB7,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB5}),arr({&mockMBB3,&mockMBB4,&mockMBB6}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB5}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}),arr({&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB3,&mockMBB4}),arr({&mockMBB2}));
  set_preds_succs(mockMBB6, arr({&mockMBB2}),arr({&mockMBB6,&mockMBB7}));
  set_preds_succs(mockMBB7, arr({&mockMBB6}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2,&mockMBB3,&mockMBB4,&mockMBB5 },
      { &mockMBB5 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(nullptr,
      { &mockMBB6 },
      { &mockMBB6 },
      { &mockMBB6 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB7],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5, &mockMBB6, &mockMBB7
    })
  );
}

TEST(boundedDominatorsAnalysisTest, TwoConsecutiveLoopsBranchedConstVar){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  mockMBB(mockMBB6,false);
  mockMBB(mockMBB7,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB5}),arr({&mockMBB3,&mockMBB4,&mockMBB6}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB5}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}),arr({&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB3,&mockMBB4}),arr({&mockMBB2}));
  set_preds_succs(mockMBB6, arr({&mockMBB2}),arr({&mockMBB6,&mockMBB7}));
  set_preds_succs(mockMBB7, arr({&mockMBB6}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2,&mockMBB3,&mockMBB4,&mockMBB5 },
      { &mockMBB5 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(nullptr,
      { &mockMBB6 },
      { &mockMBB6 },
      { &mockMBB6 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB7],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5, &mockMBB7
    })
  );
}

TEST(boundedDominatorsAnalysisTest, TwoConsecutiveLoopsBranchedVarConst){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  mockMBB(mockMBB6,true);
  mockMBB(mockMBB7,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB5}),arr({&mockMBB3,&mockMBB4,&mockMBB6}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB5}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}),arr({&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB3,&mockMBB4}),arr({&mockMBB2}));
  set_preds_succs(mockMBB6, arr({&mockMBB2}),arr({&mockMBB6,&mockMBB7}));
  set_preds_succs(mockMBB7, arr({&mockMBB6}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2,&mockMBB3,&mockMBB4,&mockMBB5 },
      { &mockMBB5 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(nullptr,
      { &mockMBB6 },
      { &mockMBB6 },
      { &mockMBB6 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB7],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB6, &mockMBB7
    })
  );
}

TEST(boundedDominatorsAnalysisTest, LoopExitToNonEnd){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = boundedDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

    EXPECT_THAT(result, SizeIs(1));
    EXPECT_THAT(
      result[&mockMBB4],
      UnorderedElementsAreArray({
        &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
      })
    );
}
}
