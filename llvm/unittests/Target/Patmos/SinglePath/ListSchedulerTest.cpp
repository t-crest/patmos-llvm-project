#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "SinglePath/SPListScheduler.h"
#include "llvm/ADT/SmallVector.h"
#include <set>

using namespace llvm;

using ::testing::Return;
using ::testing::Contains;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;
using ::testing::Eq;

namespace llvm{
  enum Operand {
    R0=0,
    R1=1,
    R2=2,
    R3=3,
    R4=4,
  };

  class MockInstr {
  public:
    /// Set of operands this instruction reads.
    std::set<Operand> reads;

    /// Set of operands this instruction writes.
    std::set<Operand> writes;

    /// Whether the outputs are poisoned until they are ready.
    /// If latency is 0, has no effect.
    /// Otherwise, if true, while the instruction is executing the writes of the instruction
    /// cannot be read/written by any other instruction
    bool poisons;

    /// If true, this instruction performs a memory access (either read or write), otherwise not.
    bool memory_access;

    /// How many cycles it takes to execute the instruction.
    /// 0 means the result is ready in the following cycles.
    /// 1 means 1 cycles must be skipped before the result is ready
    unsigned latency;

    /// Whether this instruction is a conditional branch out of the MBB
    bool conditional_branch;

    MockInstr(std::set<Operand> reads, std::set<Operand> writes, unsigned latency, bool poisons,
        bool memory_access, bool conditional_branch):
      reads(reads), writes(writes), poisons(poisons), memory_access(memory_access), latency(latency),
      conditional_branch(conditional_branch)
    {}

    bool is_read(Operand r) {
      return reads.count(r);
    }

    bool is_written(Operand r) {
      return writes.count(r);
    }

    static MockInstr nop(){
      return MockInstr({},{},0,false,false,false);
    }
  };

  bool is_constant(Operand p) {
    return p == Operand::R0;
  }

  std::set<Operand> reads(const MockInstr* instr) {
    return instr->reads;
  }

  std::set<Operand> writes(const MockInstr* instr) {
    return instr->writes;
  }

  bool poisons(const MockInstr* instr) {
    return instr->poisons;
  }

  bool memory_access(const MockInstr* instr) {
    return instr->memory_access;
  }

  unsigned latency(const MockInstr* instr) {
    return instr->latency;
  }

  bool conditional_branch(const MockInstr* instr) {
    return instr->conditional_branch;
  }

  class MockMBB {
  public:
    // We use pointers to vectors because using vectors directly always
    // resulted in segmentation fault. Don't know why. This is a workaround.
    SmallVector<MockInstr> *instructions;

    MockMBB(SmallVector<MockInstr> *instructions) : instructions(instructions) {
    }

    typename SmallVector<MockInstr>::const_iterator begin() const {
      return instructions->begin();
    }
    typename SmallVector<MockInstr>::const_iterator end() const {
      return instructions->end();
    }
    unsigned size() const {
      return instructions->size();
    }

  };
}

namespace {

#define pair(p1,p2) std::make_pair(p1,p2)
#define block(name, instrs) \
  SmallVector<MockInstr> name ## _instructions = instrs; \
  MockMBB name(&name ## _instructions);
/// Used to protect literal arrays from the preprocessor.
/// see https://stackoverflow.com/questions/5503362/passing-array-literal-as-macro-argument
#define arr(...) __VA_ARGS__

TEST(ListSchedulerTest, UnrelatedInstructions){
  /* Tests that when no dependencies occur, the order the instructions come in is maintained
   */
  block(mockMBB, arr({
    MockInstr({Operand::R0},{Operand::R0},0,false,false,false),
    MockInstr({Operand::R1},{Operand::R1},0,false,false,false),
    MockInstr({Operand::R2},{Operand::R2},0,false,false,false),
    MockInstr({Operand::R3},{Operand::R3},0,false,false,false),
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(0, 0),
        pair(1, 1),
        pair(2, 2),
        pair(3, 3),
      })
  );
}

TEST(ListSchedulerTest, PoisonAddsNop){
  /* Tests that a nop is added in delay slots if nothing else can be put there.
   */
  block(mockMBB, arr({
    MockInstr({Operand::R0},{Operand::R1},2,true,false,false),
    MockInstr({Operand::R1},{Operand::R2},0,false,false,false),
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(0, 0),
        pair(3, 1),
      })
  );
}

TEST(ListSchedulerTest, DelayAddsNop){
  /* Tests that a nop is added in delay slots if nothing else can be put there.
   */
  block(mockMBB, arr({
    MockInstr({Operand::R0},{Operand::R1},1,false,false,false),
    MockInstr({Operand::R1},{Operand::R2},0,false,false,false),
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(0, 0),
        pair(2, 1),
      })
  );
}

TEST(ListSchedulerTest, FillDelay){
  /* Tests that an unrelated instruction is put in delay slots
   */
  block(mockMBB, arr({
    MockInstr({Operand::R0},{Operand::R1},1,true,false,false),
    MockInstr({Operand::R1},{Operand::R2},0,false,false,false),
    MockInstr({Operand::R3},{Operand::R3},0,false,false,false),
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(0, 0),
        pair(2, 1),
        pair(1, 2),
      })
  );
}

TEST(ListSchedulerTest, PreferLongDelay){
  /* Tests if multiple instruction are available and unrelated, the one with the longest delay
   * is chosen first
   */
  block(mockMBB, arr({
    MockInstr({Operand::R0},{Operand::R0},0,false,false,false),
    MockInstr({Operand::R1},{Operand::R1},1,false,false,false),
    MockInstr({Operand::R2},{Operand::R3},2,false,false,false),
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(2, 0),
        pair(1, 1),
        pair(0, 2),
      })
  );
}

TEST(ListSchedulerTest, MaintainMemAccessOrder){
  /* Tests instructions accessing memory aren't reordered, even if they could have been otherwise.
   *
   * Here, all instructions access memory. The second uses the output of the first. but the third
   * is unrelated to both.
   * If the third wasn't a memory access, it would have been scheduled between the first and second.
   * However, in this case it must stay as the third
   */
  block(mockMBB, arr({
    MockInstr({Operand::R0},{Operand::R1},1,false,true,false),
    MockInstr({Operand::R1},{Operand::R2},0,false,true,false),
    MockInstr({Operand::R3},{Operand::R3},0,false,true,false),
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(0, 0),
        pair(2, 1),
        pair(3, 2),
      })
  );
}

TEST(ListSchedulerTest, ConstantsCantPoison){
  /* Tests that constants can't be poisoned
   */
  block(mockMBB, arr({
    MockInstr({Operand::R1},{Operand::R0},1,true,false,false),
    MockInstr({Operand::R0},{Operand::R2},0,false,false,false),
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(0, 0),
        pair(1, 1),
      })
  );
}

TEST(ListSchedulerTest, ConstantsDontDelay){
  /* Tests that writing to a constant with delay doesn't incur the delay from users of the constant
   */
  block(mockMBB, arr({
    MockInstr({Operand::R1},{Operand::R0},2,false,false,false),
    MockInstr({Operand::R0},{Operand::R2},0,false,false,false),
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(0, 0),
        pair(1, 1),
      })
  );
}

TEST(ListSchedulerTest, ConstantsDontMakeDependence){
  /* Tests that instruction reading/writing to a constant can be reordered
   */
  block(mockMBB, arr({
    MockInstr({Operand::R1},{Operand::R0},0,false,false,false),
    MockInstr({Operand::R0},{Operand::R2},2,false,false,false),
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(1, 0),
        pair(0, 1),
      })
  );
}

TEST(ListSchedulerTest, WeakDepInDelay){
  /* Test that a weak dependency on a delayed instruction can be scheduled in the delay slot
   */
  block(mockMBB, arr({
    MockInstr({Operand::R2},{Operand::R2},1,true,true,false),
    MockInstr({Operand::R2, Operand::R1},{Operand::R1},0,false,false,false),
    MockInstr({Operand::R0},{Operand::R4},1,true,true,false),
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(0, 0),
        pair(2, 1),
        pair(1, 2),
      })
  );
}

TEST(ListSchedulerTest, Branch){
  /* Tests a branch instruction disallows following instruction to be moved before it
   */
  block(mockMBB, arr({
    MockInstr({Operand::R0},{Operand::R1},0,false,false,false),
    MockInstr({Operand::R1},{Operand::R1},3,false,false,false),
    MockInstr({Operand::R1},{},0,false,false,true),
    MockInstr({Operand::R2},{Operand::R2},0,false,false,false),
    MockInstr({Operand::R3},{Operand::R3},4,false,false,false),
    MockInstr({Operand::R4},{Operand::R4},0,false,false,false),
    MockInstr({Operand::R3},{},0,false,false,true),
    MockInstr({Operand::R2},{Operand::R2},0,false,false,false),
    MockInstr({Operand::R3},{Operand::R3},0,false,false,false),
    MockInstr({Operand::R4},{Operand::R4},0,false,false,false)
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(0, 0),
        pair(1, 1),
        pair(5, 2),
        pair(7, 3),
        pair(6, 4),
        pair(8, 5),
        pair(11, 6),
        pair(12, 7),
        pair(13, 8),
        pair(14, 9),
      })
  );
}

TEST(ListSchedulerTest, WritesToSameArentReordered){
  /* Tests that instructions that to the same operand aren't reordered, even if
   * it results in an unused delay slot
   */
  block(mockMBB, arr({
    MockInstr({Operand::R0},{Operand::R1},1,true,true,false),
    MockInstr({Operand::R1},{Operand::R2},0,false,false,false),
    MockInstr({Operand::R3},{Operand::R2},0,false,false,false)
  }));

  auto new_schedule = list_schedule(mockMBB.begin(), mockMBB.end(),
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch);

  EXPECT_THAT(
      new_schedule,
      UnorderedElementsAreArray({
        pair(0, 0),
        pair(2, 1),
        pair(3, 2),
      })
  );
}

}
