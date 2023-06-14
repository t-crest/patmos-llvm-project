#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "SinglePath/ConstantLoopDominatorAnalysis.h"
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

TEST(ConstantLoopDominatorsAnalysisTest, OneMBB){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  set_preds_succs(mockMBB1, arr({}),arr({}));

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, Consecutive){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({}));

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(2));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, Branch){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2, &mockMBB3}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB2, &mockMBB3}), arr({}));

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, Branch2){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(6));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
  EXPECT_THAT(
    result[&mockMBB6],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5, &mockMBB6
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, Branch3){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopOneMBBVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(3));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB3
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopOneMBBConstant){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(3));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopTwoMBBVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopTwoMBBConstant){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopTwoLatchesConstant){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopTwoLatchesVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopTwoLatchesConstant2){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB4,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}),arr({&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3,&mockMBB4 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopTwoExitsConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB2,&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      {&mockMBB2,&mockMBB3},
	  {&mockMBB3},
	  {&mockMBB2,&mockMBB3}
  );
  LI.add_loop(&mockLoop1);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopTwoExitsConstant2){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB4, &mockMBB5}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2,&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      {&mockMBB2,&mockMBB3,&mockMBB4},
	  {&mockMBB4},
	  {&mockMBB2,&mockMBB3}
  );
  LI.add_loop(&mockLoop1);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB4, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopTwoExitsConstant3){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB2,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB2,&mockMBB4}),arr({}));

  MockLoop mockLoop1(nullptr,
      {&mockMBB2,&mockMBB3,&mockMBB4},
	  {&mockMBB4},
	  {&mockMBB2,&mockMBB4}
  );
  LI.add_loop(&mockLoop1);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopNonHeadExitConstant){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopNonHeadExitVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, NestedLoopConstantConstant){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
	  &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, NestedLoopConstantVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
	  &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, NestedLoopVariableVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
	  &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
	  &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
	  &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
	  &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, NestedLoopVariableConstant){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
	  &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
	  &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
	  &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
	  &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, TwoNonLatchHeaderPredsConstant){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, TwoNonLatchHeaderPredsVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopExitFromNestedConstantConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,true);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB4}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB2,&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB4}), arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
	  { &mockMBB4 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopExitFromNestedConstantConstant2){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,true);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB4}), arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3 },
	  { &mockMBB4 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3, &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopExitFromNestedConstantConstant3){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopExitFromNestedConstantVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopLatchFromNestedConstantConstant){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,true);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB2,&mockMBB3}));
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopLatchFromNestedConstantVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, TwoConsecutiveLoopsConstantConstant){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, TwoConsecutiveLoopsConstantVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, TwoConsecutiveLoopsVariableConstant){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB3, &mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, TwoConsecutiveLoopsVariableVariable){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, TwoConsecutiveLoopsBranchedConstConst){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(7));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
  EXPECT_THAT(
    result[&mockMBB6],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5, &mockMBB6
    })
  );
  EXPECT_THAT(
    result[&mockMBB7],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5, &mockMBB6, &mockMBB7
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, TwoConsecutiveLoopsBranchedConstVar){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(7));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
  EXPECT_THAT(
    result[&mockMBB6],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
  EXPECT_THAT(
    result[&mockMBB7],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5, &mockMBB7
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, TwoConsecutiveLoopsBranchedVarConst){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(7));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB6],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB6
    })
  );
  EXPECT_THAT(
    result[&mockMBB7],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB6, &mockMBB7
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopExitToNonEnd){
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

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

    EXPECT_THAT(result, SizeIs(4));
    EXPECT_THAT(
      result[&mockMBB1],
      UnorderedElementsAreArray({
        &mockMBB1
      })
    );
    EXPECT_THAT(
      result[&mockMBB2],
      UnorderedElementsAreArray({
        &mockMBB1, &mockMBB2,
      })
    );
    EXPECT_THAT(
      result[&mockMBB3],
      UnorderedElementsAreArray({
        &mockMBB1, &mockMBB2, &mockMBB3
      })
    );
    EXPECT_THAT(
      result[&mockMBB4],
      UnorderedElementsAreArray({
        &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
      })
    );
}

TEST(ConstantLoopDominatorsAnalysisTest, DeepVariableLoopExit){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,true);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2, &mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB4,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB4}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2,&mockMBB3,&mockMBB4 },
      { &mockMBB3 },
      { &mockMBB4 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3,&mockMBB4 },
      { &mockMBB4 },
      { &mockMBB4 }
  );
  MockLoop mockLoop3(&mockLoop2,
      { &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);
  LI.add_loop(&mockLoop3);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2,
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, DeepConstantVariableLoopLatch){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2, &mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3,&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2,&mockMBB3,&mockMBB4 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3,&mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3,&mockMBB4 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, DeepConstantLoopExitVariableParent){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,true);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2, &mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB4,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB4}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2,&mockMBB3,&mockMBB4 },
      { &mockMBB3 },
      { &mockMBB4 }
  );
  MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3,&mockMBB4 },
      { &mockMBB4 },
      { &mockMBB4 }
  );
  MockLoop mockLoop3(&mockLoop2,
      { &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);
  LI.add_loop(&mockLoop3);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB1],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopBodyDominatedByOutside){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2, &mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2,&mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds, false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3,&mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LoopBodyDominatedByOutside2){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2, &mockMBB4, &mockMBB5}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2,&mockMBB3,&mockMBB4 },
      { &mockMBB3,&mockMBB4 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds, false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3,&mockMBB4
    })
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
      &mockMBB1, &mockMBB2, &mockMBB3,&mockMBB5
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, VariableLoopBodyNotDominatedByLoop){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2, &mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2,&mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds, false);

  EXPECT_THAT(result, SizeIs(4));
  EXPECT_THAT(
    result[&mockMBB2],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB3],
    UnorderedElementsAreArray({
      &mockMBB1
    })
  );
  EXPECT_THAT(
    result[&mockMBB4],
    UnorderedElementsAreArray({
      &mockMBB1,&mockMBB4
    })
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, TwoLoopExitTargets){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB5}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}),arr({&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB3,&mockMBB4}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(
	result[&mockMBB1],
	UnorderedElementsAreArray({
	  &mockMBB1
	})
  );
  EXPECT_THAT(
	result[&mockMBB2],
	UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2
	})
  );
  EXPECT_THAT(
	result[&mockMBB3],
	UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2, &mockMBB3
	})
  );
  EXPECT_THAT(
	result[&mockMBB4],
	UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2, &mockMBB4
	})
  );
  EXPECT_THAT(
    result[&mockMBB5],
    UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2, &mockMBB5
    })
  );

  // Try with variable loop
  mockMBB2.payload = false;
  auto result2 = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(
	result2[&mockMBB5],
	UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB5
	})
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, LongBranch){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,false);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  mockMBB(mockMBB6,false);
  mockMBB(mockMBB7,false);
  mockMBB(mockMBB8,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2, &mockMBB3}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB7}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB4}),arr({&mockMBB6}));
  set_preds_succs(mockMBB6, arr({&mockMBB5}),arr({&mockMBB7}));
  set_preds_succs(mockMBB7, arr({&mockMBB2, &mockMBB6}),arr({&mockMBB8}));
  set_preds_succs(mockMBB8, arr({&mockMBB7}),arr({}));

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds);

  EXPECT_THAT(result, SizeIs(1));
  EXPECT_THAT(
	result[&mockMBB8],
	UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB7, &mockMBB8
	})
  );
}

TEST(ConstantLoopDominatorsAnalysisTest, CLDAPaperExample){
  MockLoopInfo LI;

  mockMBB(mockMBB1,false);
  mockMBB(mockMBB2,true);
  mockMBB(mockMBB3,false);
  mockMBB(mockMBB4,false);
  mockMBB(mockMBB5,false);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB4,&mockMBB5}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB3}),arr({}));

  MockLoop mockLoop1(nullptr,
      { &mockMBB2,&mockMBB3,&mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);

  auto result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
  EXPECT_THAT(
	result[&mockMBB1],
	UnorderedElementsAreArray({
	  &mockMBB1
	})
  );
  EXPECT_THAT(
	result[&mockMBB2],
	UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2,
	})
  );
  EXPECT_THAT(
	result[&mockMBB3],
	UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2, &mockMBB3
	})
  );
  EXPECT_THAT(
	result[&mockMBB4],
	UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4
	})
  );
  EXPECT_THAT(
	result[&mockMBB5],
	UnorderedElementsAreArray({
	  &mockMBB1, &mockMBB2, &mockMBB3, &mockMBB4, &mockMBB5
	})
  );

  // Test variable loop
  mockMBB2.payload = false;

  result = constantLoopDominatorsAnalysis(&mockMBB1,&LI,constantBounds,false);

  EXPECT_THAT(result, SizeIs(5));
    EXPECT_THAT(
  	result[&mockMBB1],
  	UnorderedElementsAreArray({
  	  &mockMBB1
  	})
  );
  EXPECT_THAT(
  	result[&mockMBB2],
  	UnorderedElementsAreArray({
  	  &mockMBB1
  	})
  );
  EXPECT_THAT(
  	result[&mockMBB3],
  	UnorderedElementsAreArray({
  	  &mockMBB1
  	})
  );
  EXPECT_THAT(
  	result[&mockMBB4],
  	UnorderedElementsAreArray({
  	  &mockMBB1
  	})
  );
  EXPECT_THAT(
  	result[&mockMBB5],
  	UnorderedElementsAreArray({
  	  &mockMBB1, &mockMBB5
  	})
  );
}
}
