#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "SinglePath/MemoryAccessAnalysis.h"

using namespace llvm;

using ::testing::Return;
using ::testing::Contains;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;

namespace llvm{

  /// following function was copied from:
  ///https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
  template<typename ... Args>
  std::string string_format( const std::string& format, Args ... args )
  {
      int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
      assert(size_s > 0 );
      auto size = static_cast<size_t>( size_s );
      std::unique_ptr<char[]> buf( new char[ size ] );
      std::snprintf( buf.get(), size, format.c_str(), args ... );
      return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
  }

  class MockMBB {
  public:

    MockMBB(unsigned access_count, int iter_max):
        access_count(access_count),
        iter_max(iter_max),
        preds(nullptr), succs(nullptr)
    {}

    unsigned access_count;
	int iter_max;

	// We use pointers to vectors because using vectors directly always
	// resulted in segmentation fault. Don't know why. This is a workaround.
    std::vector<MockMBB*> *preds;
	std::vector<MockMBB*> *succs;
	
	void set_preds(std::vector<MockMBB*> *new_preds){
	  preds = new_preds;
	}
	
	void set_succs(std::vector<MockMBB*> *new_succs){
	    succs = new_succs;
	}
	
	std::vector<MockMBB*>::const_iterator pred_begin() const {
		return preds->begin();
	}
	
	std::vector<MockMBB*>::const_iterator pred_end() const {
		return preds->end();
	}
	
	unsigned pred_size() const {
		return preds->size();
	}
	
	std::vector<MockMBB*>::const_iterator succ_begin() const {
		return succs->begin();
	}
	
	std::vector<MockMBB*>::const_iterator succ_end() const {
		return succs->end();
	}
	
	std::string getName() const {
		return string_format("MockMBB(%d,%d)", access_count, iter_max);
	}
  };
  
  unsigned countAccess(const MockMBB *mbb){
	  return mbb->access_count;
  }
  
  uint64_t iterMax(const MockMBB *mbb){
	  return mbb->iter_max;
  }
    
  class MockLoop {
  public:

    const MockLoop *parent;
	std::vector<MockMBB*> mbbs;
	std::vector<MockMBB*> latches;
	std::vector<MockMBB*> exits;

    MockLoop( const MockLoop *parent,
        std::vector<MockMBB*> new_mbbs,
        std::vector<MockMBB*> new_latches,
        std::vector<MockMBB*> new_exits
	): parent(parent), mbbs(new_mbbs), latches(new_latches), exits(new_exits)
    {}
	
	MockMBB* getHeader() const {
		return mbbs[0];
	}

    void getExitingBlocks(SmallVectorImpl<MockMBB *> &out) const {
        out.append(exits.begin(),exits.end());
    }

    void getExitEdges(SmallVectorImpl<std::pair<MockMBB *,MockMBB *>> &out) const {
        SmallVector<MockMBB*> loop_exits;
        getExitingBlocks(loop_exits);
        for(auto *exit_block: loop_exits){
          std::for_each(exit_block->succ_begin(), exit_block->succ_end(), [&](auto succ){
            if(!contains(succ)) {
              out.push_back(std::make_pair(exit_block, succ));
            }
          });
        }
    }

    void getLoopLatches(SmallVectorImpl<MockMBB *> &out) const {
        out.append(latches.begin(),latches.end());
    }
	
	bool contains(const MockMBB *mbb) const {
		return std::find(mbbs.begin(),mbbs.end(),mbb) != mbbs.end();
	}
	
	bool isLoopLatch(const MockMBB *mbb) const {
	    assert(contains(mbb) && "isLoopLatch must only be called on blocks that are part of the loop");
		return std::find(latches.begin(),latches.end(),mbb) != latches.end();
	}
	
	bool isLoopExiting(const MockMBB *mbb) const {
	    assert(contains(mbb) && "isLoopExiting must only be called on blocks that are part of the loop");
		return std::find(exits.begin(),exits.end(),mbb) != exits.end();
	}

    unsigned getLoopDepth() const {
        if(!parent){
          return 1;
        } else {
          return parent->getLoopDepth() + 1;
        }
    }

    const MockLoop* getParentLoop() const {
      return parent;
    }

  };
  
  class MockLoopInfo {
  public:

	std::vector<MockLoop*> loops;
    MockLoopInfo()
    {}
	
	void add_loop(MockLoop* l) {
		loops.push_back(l);
	}
	
	const MockLoop* getLoopFor(const MockMBB *mbb) const {
	    unsigned depth = 0;
	    const MockLoop* result = nullptr;
	    while(true) {
          auto found = std::find_if(loops.begin(), loops.end(),
              [&](auto l){ return l->contains(mbb) && l->getLoopDepth() > depth;});
          if(found != loops.end()) {
            result = *found;
            depth = result->getLoopDepth();
          } else {
              break;
          }
	    }
	    return result;
	}
	
	bool isLoopHeader(const MockMBB *mbb) const {
		auto found = std::find_if(loops.begin(), loops.end(), 
			[&](auto l){ return l->getHeader() == mbb;});
		return found != loops.end();
	}
	
	unsigned getLoopDepth(const MockMBB *mbb) const {
		auto loop = getLoopFor(mbb);
		if(loop) {
			return loop->getLoopDepth();
		} else {
			return 0;
		}
	}

    std::vector<MockLoop*>::const_iterator begin() const{
      return loops.begin();
    }

    std::vector<MockLoop*>::const_iterator end() const{
      return loops.end();
    }
  };
}

namespace {

/// Used to protect literal arrays from the preprocessor.
/// see https://stackoverflow.com/questions/5503362/passing-array-literal-as-macro-argument
#define arr(...) __VA_ARGS__
/// Easy way to define the predecessors and successors of a block.
/// Lists must be given in braces '{...}'
#define set_preds_succs(block, preds, succs) \
  std::vector<MockMBB*> block ## _pred = preds; \
  std::vector<MockMBB*> block ## _succ = succs; \
  block.set_preds(& block ## _pred); \
  block.set_succs(& block ## _succ);

TEST(MemoryAccessAnalysisTest, OneMBB){
  MockLoopInfo LI;

  MockMBB mockMBB1(0,-1);
  set_preds_succs(mockMBB1, arr({}),arr({}));

  auto result1 = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
    result1.first,
    UnorderedElementsAreArray({
      std::make_pair(&mockMBB1, 0),
    })
  );
  EXPECT_THAT(result1.second,IsEmpty());

  MockMBB mockMBB2(1,-1);
  set_preds_succs(mockMBB2, arr({}), arr({}));

  auto result2 = memoryAccessAnalysis(&mockMBB2,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result2.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB2, 1),
      })
  );
  EXPECT_THAT(result2.second,IsEmpty());

  MockMBB mockMBB3(1345,-1);
  set_preds_succs(mockMBB3, arr({}), arr({}));

  auto result3 = memoryAccessAnalysis(&mockMBB3,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result3.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB3, 1345),
      })
  );
  EXPECT_THAT(result3.second,IsEmpty());
}

TEST(MemoryAccessAnalysisTest, Consecutive){
  MockLoopInfo LI;

  MockMBB mockMBB1(3,-1);
  MockMBB mockMBB2(4,-1);
  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({}));

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 3),
        std::make_pair(&mockMBB2, 7),
      })
  );
  EXPECT_THAT(result.second,IsEmpty());
}

TEST(MemoryAccessAnalysisTest, FirstCountZero){
  MockLoopInfo LI;

  MockMBB mockMBB1(0,-1);
  MockMBB mockMBB2(6,-1);
  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({}));

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 0),
        std::make_pair(&mockMBB2, 6),
      })
  );
  EXPECT_THAT(result.second,IsEmpty());
}

TEST(MemoryAccessAnalysisTest, Branch){
  MockLoopInfo LI;

  MockMBB mockMBB1(1,-1);
  MockMBB mockMBB2(2,-1);
  MockMBB mockMBB3(3,-1);
  MockMBB mockMBB4(4,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2, &mockMBB3}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB2, &mockMBB3}), arr({}));

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 1),
        std::make_pair(&mockMBB2, 3),
        std::make_pair(&mockMBB3, 4),
        std::make_pair(&mockMBB4, 8),
      })
  );
  EXPECT_THAT(result.second,IsEmpty());
}

TEST(MemoryAccessAnalysisTest, Loop){
  MockLoopInfo LI;

  MockMBB mockMBB1(1,-1);
  MockMBB mockMBB2(2, 4);
  MockMBB mockMBB3(3,-1);
  MockMBB mockMBB4(4,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}), arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 1),
        std::make_pair(&mockMBB2, 2),
        std::make_pair(&mockMBB3, 5),
        std::make_pair(&mockMBB4, 22),
      })
  );
  EXPECT_THAT(result.second, SizeIs(1));
  EXPECT_THAT(std::get<0>(result.second[&mockMBB2]), Eq(1));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB2]), Eq(5));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB2]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB2, 18),
      })
  );
}

TEST(MemoryAccessAnalysisTest, LoopZeroHeader){
  MockLoopInfo LI;

  MockMBB mockMBB1(1,-1);
  MockMBB mockMBB2(0, 4);
  MockMBB mockMBB3(3,-1);
  MockMBB mockMBB4(4,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}), arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 1),
        std::make_pair(&mockMBB2, 0),
        std::make_pair(&mockMBB3, 3),
        std::make_pair(&mockMBB4, 14),
      })
  );
  EXPECT_THAT(result.second, SizeIs(1));
  EXPECT_THAT(std::get<0>(result.second[&mockMBB2]), Eq(1));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB2]), Eq(3));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB2]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB2, 10),
      })
  );
}

TEST(MemoryAccessAnalysisTest, LoopOneMBB){
  MockLoopInfo LI;

  MockMBB mockMBB1(1,-1);
  MockMBB mockMBB2(2, 8);
  MockMBB mockMBB3(3,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 1),
        std::make_pair(&mockMBB2, 2),
        std::make_pair(&mockMBB3, 20),
      })
  );
  EXPECT_THAT(result.second, SizeIs(1));
  EXPECT_THAT(std::get<0>(result.second[&mockMBB2]), Eq(1));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB2]), Eq(2));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB2]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB2, 17),
      })
  );
}

TEST(MemoryAccessAnalysisTest, LoopNonHeaderExit){
  MockLoopInfo LI;

  MockMBB mockMBB1(1,-1);
  MockMBB mockMBB2(2, 2);
  MockMBB mockMBB3(3,-1);
  MockMBB mockMBB4(4,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}), arr({}));
  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 1),
        std::make_pair(&mockMBB2, 2),
        std::make_pair(&mockMBB3, 5),
        std::make_pair(&mockMBB4, 15),
      })
  );
  EXPECT_THAT(result.second, SizeIs(1));
  EXPECT_THAT(std::get<0>(result.second[&mockMBB2]), Eq(1));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB2]), Eq(5));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB2]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB3, 11),
      })
  );
}

TEST(MemoryAccessAnalysisTest, LoopNonHeaderNonLatchExit){
  MockLoopInfo LI;

  MockMBB mockMBB1(0,-1);
  MockMBB mockMBB2(2, 3);
  MockMBB mockMBB3(0,-1);
  MockMBB mockMBB4(4,-1);
  MockMBB mockMBB5(2,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB4,&mockMBB5}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}), arr({&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB3}), arr({}));
  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 0),
        std::make_pair(&mockMBB2, 2),
        std::make_pair(&mockMBB3, 2),
        std::make_pair(&mockMBB4, 6),
        std::make_pair(&mockMBB5, 16),
      })
  );
  EXPECT_THAT(result.second, SizeIs(1));
  EXPECT_THAT(std::get<0>(result.second[&mockMBB2]), Eq(0));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB2]), Eq(6));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB2]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB3, 14),
      })
  );
}

TEST(MemoryAccessAnalysisTest, LoopMultipleLatches){
  MockLoopInfo LI;

  MockMBB mockMBB1(1,-1);
  MockMBB mockMBB2(2, 7);
  MockMBB mockMBB3(3,-1);
  MockMBB mockMBB4(4,-1);
  MockMBB mockMBB5(5,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB4,&mockMBB2}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3, &mockMBB4 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 1),
        std::make_pair(&mockMBB2, 2),
        std::make_pair(&mockMBB3, 5),
        std::make_pair(&mockMBB4, 9),
        std::make_pair(&mockMBB5, 62),
      })
  );
  EXPECT_THAT(result.second, SizeIs(1));
  EXPECT_THAT(std::get<0>(result.second[&mockMBB2]), Eq(1));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB2]), Eq(9));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB2]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB2, 57),
      })
  );
}

TEST(MemoryAccessAnalysisTest, LoopNested){
  MockLoopInfo LI;

  MockMBB mockMBB1(10,-1);
  MockMBB mockMBB2(20,7);
  MockMBB mockMBB3(30,3);
  MockMBB mockMBB4(40,-1);
  MockMBB mockMBB5(50,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  llvm::MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 10),
        std::make_pair(&mockMBB2, 20),
        std::make_pair(&mockMBB3, 30),
        std::make_pair(&mockMBB4, 70),
        std::make_pair(&mockMBB5, 1220),
      })
  );
  EXPECT_THAT(result.second, SizeIs(2));
  EXPECT_THAT(std::get<0>(result.second[&mockMBB2]), Eq(10));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB2]), Eq(190));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB2]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB2, 1170),
      })
  );
  EXPECT_THAT(std::get<0>(result.second[&mockMBB3]), Eq(20));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB3]), Eq(70));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB3]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB3, 190),
      })
  );
}

TEST(MemoryAccessAnalysisTest, LoopLatchFromNested){
  MockLoopInfo LI;

  MockMBB mockMBB1(11,-1);
  MockMBB mockMBB2(22,2);
  MockMBB mockMBB3(33,4);
  MockMBB mockMBB4(44,-1);
  MockMBB mockMBB5(55,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3,&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      {&mockMBB2,&mockMBB3,&mockMBB4},
      {&mockMBB3,&mockMBB4},
      {&mockMBB2}
  );
  llvm::MockLoop mockLoop2(&mockLoop1,
      {&mockMBB3,&mockMBB4},
      {&mockMBB4},
      {&mockMBB3,&mockMBB4}
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 11),
        std::make_pair(&mockMBB2, 22),
        std::make_pair(&mockMBB3, 33),
        std::make_pair(&mockMBB4, 77),
        std::make_pair(&mockMBB5, 418),
      })
  );
  EXPECT_THAT(result.second, SizeIs(2));
  EXPECT_THAT(std::get<0>(result.second[&mockMBB2]), Eq(11));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB2]), Eq(330));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB2]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB2, 363),
      })
  );
  EXPECT_THAT(std::get<0>(result.second[&mockMBB3]), Eq(22));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB3]), Eq(77));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB3]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB3, 286),
        std::make_pair(&mockMBB4, 330),
      })
  );
}

TEST(MemoryAccessAnalysisTest, LoopExitFromNested){
  MockLoopInfo LI;

  MockMBB mockMBB1(12,-1);
  MockMBB mockMBB2(23,16);
  MockMBB mockMBB3(34,6);
  MockMBB mockMBB4(45,-1);
  MockMBB mockMBB5(56,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB2,&mockMBB4}), arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3 },
      { &mockMBB2, &mockMBB4 }
  );
  llvm::MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3, &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = memoryAccessAnalysis(&mockMBB1,&LI,countAccess,iterMax);
  EXPECT_THAT(
      result.first,
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB1, 12),
        std::make_pair(&mockMBB2, 23),
        std::make_pair(&mockMBB3, 34),
        std::make_pair(&mockMBB4, 79),
        std::make_pair(&mockMBB5, 8020),
      })
  );
  EXPECT_THAT(result.second, SizeIs(2));
  EXPECT_THAT(std::get<0>(result.second[&mockMBB2]), Eq(12));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB2]), Eq(497));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB2]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB2, 7490),
        std::make_pair(&mockMBB4, 7964),
      })
  );
  EXPECT_THAT(std::get<0>(result.second[&mockMBB3]), Eq(23));
  EXPECT_THAT(std::get<1>(result.second[&mockMBB3]), Eq(79));
  EXPECT_THAT(
      std::get<2>(result.second[&mockMBB3]),
      UnorderedElementsAreArray({
        std::make_pair(&mockMBB3, 452),
        std::make_pair(&mockMBB4, 497),
      })
  );
}

TEST(MemoryAccessCompensationTest, OneMBB){
  MockLoopInfo LI;

  MockMBB mockMBB1(9,-1);
  set_preds_succs(mockMBB1, arr({}),arr({}));

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(result,IsEmpty());
}

TEST(MemoryAccessCompensationTest, TwoMBB){
  MockLoopInfo LI;

  MockMBB mockMBB1(9,-1);
  MockMBB mockMBB2(4,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({}));

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB1,&mockMBB2), 13),
      })
  );
}

TEST(MemoryAccessCompensationTest, FirstCountZero){
  MockLoopInfo LI;

  MockMBB mockMBB1(0,-1);
  MockMBB mockMBB2(4,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({}));

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB1,&mockMBB2), 4),
      })
  );
}

TEST(MemoryAccessCompensationTest, AllZero){
  MockLoopInfo LI;

  MockMBB mockMBB1(0,-1);
  MockMBB mockMBB2(0,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({}));

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(result, IsEmpty());
}

TEST(MemoryAccessCompensationTest, TwoReturns){
  MockLoopInfo LI;

  MockMBB mockMBB1(3,-1);
  MockMBB mockMBB2(24,-1);
  MockMBB mockMBB3(5,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2, &mockMBB3}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({}));

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB1,&mockMBB2), 27),
        std::make_pair(std::make_pair(&mockMBB1,&mockMBB3), 8),
      })
  );
}

TEST(MemoryAccessCompensationTest, Merge){
  MockLoopInfo LI;

  MockMBB mockMBB1(67,-1);
  MockMBB mockMBB2(2,-1);
  MockMBB mockMBB3(12,-1);
  MockMBB mockMBB4(8,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2, &mockMBB3}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB2,&mockMBB3}),arr({}));

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB4), 77),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB4), 87),
      })
  );
}

TEST(MemoryAccessCompensationTest, MergeTwoReturns){
  MockLoopInfo LI;

  MockMBB mockMBB1(14,-1);
  MockMBB mockMBB2(25,-1);
  MockMBB mockMBB3(0,-1);
  MockMBB mockMBB4(23,-1);
  MockMBB mockMBB5(2,-1);
  MockMBB mockMBB6(4,-1);
  MockMBB mockMBB7(16,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2, &mockMBB3, &mockMBB4, &mockMBB5}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB6}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB6}));
  set_preds_succs(mockMBB4, arr({&mockMBB1}),arr({&mockMBB7}));
  set_preds_succs(mockMBB5, arr({&mockMBB1}),arr({&mockMBB7}));
  set_preds_succs(mockMBB6, arr({&mockMBB2,&mockMBB3}),arr({}));
  set_preds_succs(mockMBB7, arr({&mockMBB4,&mockMBB5}),arr({}));

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB6), 43),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB6), 18),
        std::make_pair(std::make_pair(&mockMBB4,&mockMBB7), 53),
        std::make_pair(std::make_pair(&mockMBB5,&mockMBB7), 32),
      })
  );
}

TEST(MemoryAccessCompensationTest, TwoPredecessorsToNonEnd){
  MockLoopInfo LI;

  MockMBB mockMBB1(2,-1);
  MockMBB mockMBB2(3,-1);
  MockMBB mockMBB3(4,-1);
  MockMBB mockMBB4(5,-1);
  MockMBB mockMBB5(6,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB2,&mockMBB3}),arr({&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB4}),arr({}));

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB4), 5),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB4), 6),
        std::make_pair(std::make_pair(&mockMBB4,&mockMBB5), 11),
      })
  );
}

TEST(MemoryAccessCompensationTest, OneMBBLoop){
  MockLoopInfo LI;

  MockMBB mockMBB1(54,-1);
  MockMBB mockMBB2(4,2);
  MockMBB mockMBB3(7,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB2), 4),
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB3), 65),
      })
  );
}

TEST(MemoryAccessCompensationTest, Loop){
  MockLoopInfo LI;

  MockMBB mockMBB1(4,-1);
  MockMBB mockMBB2(4,9);
  MockMBB mockMBB3(7,-1);
  MockMBB mockMBB4(25,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2}));
  set_preds_succs(mockMBB4, arr({&mockMBB2}),arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB2), 11),
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB4), 33),
      })
  );
}

TEST(MemoryAccessCompensationTest, LoopTwoLatches){
  MockLoopInfo LI;

  MockMBB mockMBB1(23,-1);
  MockMBB mockMBB2(78,9);
  MockMBB mockMBB3(45,-1);
  MockMBB mockMBB4(2,-1);
  MockMBB mockMBB5(5,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}),arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3,&mockMBB4 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB2), 123),
        std::make_pair(std::make_pair(&mockMBB4,&mockMBB2), 125),
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB5), 106),
      })
  );
}

TEST(MemoryAccessCompensationTest, LoopNonHeadExit){
  MockLoopInfo LI;

  MockMBB mockMBB1(6,-1);
  MockMBB mockMBB2(9,11);
  MockMBB mockMBB3(78,-1);
  MockMBB mockMBB4(34,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB2), 87),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB4), 127),
      })
  );
}

TEST(MemoryAccessCompensationTest, NestedLoop){
  MockLoopInfo LI;

    MockMBB mockMBB1(10,-1);
    MockMBB mockMBB2(20,7);
    MockMBB mockMBB3(30,3);
    MockMBB mockMBB4(40,-1);
    MockMBB mockMBB5(50,-1);
    set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
    set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
    set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
    set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3}));
    set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

    llvm::MockLoop mockLoop1(nullptr,
        { &mockMBB2, &mockMBB3, &mockMBB4 },
        { &mockMBB3 },
        { &mockMBB2 }
    );
    llvm::MockLoop mockLoop2(&mockLoop1,
        { &mockMBB3, &mockMBB4 },
        { &mockMBB4 },
        { &mockMBB3 }
    );
    LI.add_loop(&mockLoop1);
    LI.add_loop(&mockLoop2);

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB5), 80),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB2), 50),
        std::make_pair(std::make_pair(&mockMBB4,&mockMBB3), 70),
      })
  );
}

TEST(MemoryAccessCompensationTest, TwoNonLatchHeaderPreds){
  MockLoopInfo LI;

  MockMBB mockMBB1(1,-1);
  MockMBB mockMBB2(0,-1);
  MockMBB mockMBB3(10,-1);
  MockMBB mockMBB4(7,5);
  MockMBB mockMBB5(3,-1);

  set_preds_succs(mockMBB1, arr({}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB2, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB3, arr({&mockMBB1}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB2,&mockMBB3}),arr({&mockMBB4,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB4}),arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB4), 1),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB4), 11),
        std::make_pair(std::make_pair(&mockMBB4,&mockMBB4), 7),
        std::make_pair(std::make_pair(&mockMBB4,&mockMBB5), 10),
      })
  );
}

TEST(MemoryAccessCompensationTest, LoopExitFromNested){
  MockLoopInfo LI;

  MockMBB mockMBB1(12,-1);
  MockMBB mockMBB2(23,16);
  MockMBB mockMBB3(34,6);
  MockMBB mockMBB4(45,-1);
  MockMBB mockMBB5(56,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB5, arr({&mockMBB2,&mockMBB4}), arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2, &mockMBB3, &mockMBB4 },
      { &mockMBB3 },
      { &mockMBB2, &mockMBB4 }
  );
  llvm::MockLoop mockLoop2(&mockLoop1,
      { &mockMBB3, &mockMBB4 },
      { &mockMBB4 },
      { &mockMBB3, &mockMBB4 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB5), 91),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB2), 57),
        std::make_pair(std::make_pair(&mockMBB4,&mockMBB3), 79),
        std::make_pair(std::make_pair(&mockMBB4,&mockMBB5), 170),
      })
  );
}

TEST(MemoryAccessCompensationTest, LoopLatchFromNested){
  MockLoopInfo LI;

  MockMBB mockMBB1(11,-1);
  MockMBB mockMBB2(22,2);
  MockMBB mockMBB3(33,4);
  MockMBB mockMBB4(44,-1);
  MockMBB mockMBB5(55,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB3,&mockMBB4}),arr({&mockMBB3,&mockMBB5}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB4}),arr({&mockMBB2,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({&mockMBB3,&mockMBB2}));
  set_preds_succs(mockMBB5, arr({&mockMBB2}), arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      {&mockMBB2,&mockMBB3,&mockMBB4},
      {&mockMBB3,&mockMBB4},
      {&mockMBB2}
  );
  llvm::MockLoop mockLoop2(&mockLoop1,
      {&mockMBB3,&mockMBB4},
      {&mockMBB4},
      {&mockMBB3,&mockMBB4}
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB5), 88),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB2), 55),
        std::make_pair(std::make_pair(&mockMBB4,&mockMBB3), 77),
        std::make_pair(std::make_pair(&mockMBB4,&mockMBB2), 99),
      })
  );
}

TEST(MemoryAccessCompensationTest, TwoConsecutiveLoops){
  MockLoopInfo LI;

  MockMBB mockMBB1(3,-1);
  MockMBB mockMBB2(1, 10);
  MockMBB mockMBB3(6, 2);
  MockMBB mockMBB4(10,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2,&mockMBB3}),arr({&mockMBB3,&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  llvm::MockLoop mockLoop2(nullptr,
      { &mockMBB3 },
      { &mockMBB3 },
      { &mockMBB3 }
  );
  LI.add_loop(&mockLoop1);
  LI.add_loop(&mockLoop2);

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB2), 1),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB3), 6),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB4), 20),
      })
  );
}

TEST(MemoryAccessCompensationTest, LoopExitToNonEnd){
  MockLoopInfo LI;

  MockMBB mockMBB1(2,-1);
  MockMBB mockMBB2(1, 10);
  MockMBB mockMBB3(3, 2);
  MockMBB mockMBB4(10,-1);
  set_preds_succs(mockMBB1, arr({}), arr({&mockMBB2}));
  set_preds_succs(mockMBB2, arr({&mockMBB1,&mockMBB2}),arr({&mockMBB2,&mockMBB3}));
  set_preds_succs(mockMBB3, arr({&mockMBB2}),arr({&mockMBB4}));
  set_preds_succs(mockMBB4, arr({&mockMBB3}),arr({}));

  llvm::MockLoop mockLoop1(nullptr,
      { &mockMBB2 },
      { &mockMBB2 },
      { &mockMBB2 }
  );
  LI.add_loop(&mockLoop1);

  auto result = memoryAccessCompensation(&mockMBB1, &LI, countAccess);

  EXPECT_THAT(
      result,
      UnorderedElementsAreArray({
        std::make_pair(std::make_pair(&mockMBB2,&mockMBB2), 1),
        std::make_pair(std::make_pair(&mockMBB3,&mockMBB4), 16),
      })
  );
}

}
