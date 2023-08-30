#ifndef TARGET_PATMOS_SINGLEPATH_SPLISTSCHEDULER_H_
#define TARGET_PATMOS_SINGLEPATH_SPLISTSCHEDULER_H_

#include "llvm/Support/Debug.h"
#include "llvm/ADT/APInt.h"
#include <map>
#include <set>

#define DEBUG_TYPE "patmos-singlepath"

// Uncomment to always print debug info
//#define LLVM_DEBUG(...) __VA_ARGS__

namespace llvm {

/// A node in the instruction dependence graph.
class Node {
public:
  /// Index in the MBB of the instruction represented by this node
  unsigned idx;

  /// How many cycles are needed for its instruction to finish executing.
  /// 0 means the instruction's result is ready next cycles
  unsigned latency;

  /// Whether the instruction may be scheduled in the second issue slot.
  bool may_second_slot;

  /// Whether the instruction takes up both issue slots
  bool is_long;

  /// Predecessors in the graph.
  /// The bool is the strength of the dependency. If true, the dependency is strong, meaning
  /// this node can't be scheduled until after the predecessor finishes executing.
  /// If false, this node can be scheduled if the predecessor has already been scheduled
  /// (but might still be executing)
  std::map<std::shared_ptr<Node>, bool> preds;

  /// Successors in the graph
  std::set<std::shared_ptr<Node>> succs;

  Node(unsigned idx, unsigned latency, bool may_second_slot, bool is_long,
      std::map<std::shared_ptr<Node>, bool> preds,
      std::set<std::shared_ptr<Node>> succs
  )
    : idx(idx), latency(latency), may_second_slot(may_second_slot), is_long(is_long),
      preds(preds), succs(succs) {}

  static void dump_graph(std::set<std::shared_ptr<Node>> roots, raw_ostream& os){
    os << "Instruction Dependence Graph:\n";
    std::set<std::shared_ptr<Node>> to_print;
    std::set<std::shared_ptr<Node>> printed;
    to_print.insert(roots.begin(), roots.end());

    auto get_lowest_idx = [&](){
      auto lowest = *to_print.begin();
      for(auto node: to_print) {
        if(lowest->idx > node->idx) {
          lowest = node;
        }
      }
      to_print.erase(lowest);
      return lowest;
    };

    while(to_print.size() != 0) {
      auto next = get_lowest_idx();
      if(!printed.count(next)) {
        printed.insert(next);
        os << (roots.count(next)? "Root": "Node") << ": Index(" << next->idx << ") ";
        os << "Predecessors[";
        for(auto pred: next->preds) {
          os << "(" << pred.first->idx << ", " << (pred.second? "strong":"weak") << "), ";
        }
        os << "] Successors[";
        for(auto succ: next->succs) {
          os << succ->idx << ", ";
        }
        os << "] Latency[" << next->latency << "] MaySecondSlot["
            << next->may_second_slot << "] IsLong[" << next->is_long << "]\n";
        to_print.insert(next->succs.begin(), next->succs.end());
      }

    }
  }
};

/// Constructs the dependence graph for the given block.
/// Returns the set of root nodes that don't depend on other nodes.
///
/// Block must have at least 1 instruction
template<
  typename Instruction,
  typename InstrIter,
  typename Operand,
  typename MAY_SECOND_SLOT_EXTRA
>
std::set<std::shared_ptr<Node>> dependence_graph(
    InstrIter instr_begin, InstrIter instr_end,
    std::set<Operand> (*reads)(const Instruction *),
    std::set<Operand> (*writes)(const Instruction *),
    bool (*poisons)(const Instruction *),
    bool (*memory_access)(const Instruction *),
    unsigned (*latency)(const Instruction *),
    bool (*is_constant)(Operand),
    bool (*conditional_branch)(const Instruction *),
    Optional<std::tuple<
        MAY_SECOND_SLOT_EXTRA,
        bool (*)(MAY_SECOND_SLOT_EXTRA, const Instruction *),
        bool (*)(const Instruction *),
        bool (*)(const Instruction *,const Instruction *)
      >> enable_dual_issue
) {
  auto instr_count = std::distance(instr_begin, instr_end);
  assert(instr_count >= 1 && "Cannot create DFG for empty block");

  // Nodes that depend on no other nodes
  std::set<std::shared_ptr<Node>> roots;
  // Tracks what nodes last wrote each operand
  std::map<Operand, std::shared_ptr<Node>> last_writes;
  // Tracks what nodes have read from an operand since it was last written to.
  std::map<Operand, std::set<std::shared_ptr<Node>>> last_reads;
  // Tracks which node was the last to access memory
  Optional<std::shared_ptr<Node>> last_mem_acc = None;
  // Tracks nodes seen since the last conditional branch
  std::set<std::shared_ptr<Node>> last_non_branch;
  // Tracks the last seen conditional branch
  Optional<std::shared_ptr<Node>> last_branch = None;

  // Updates last_writes and last_reads following
  // the given node
  auto update_read_writes = [&](std::shared_ptr<Node> node){
    auto *instr = &(*std::next(instr_begin,node->idx));
    for(auto r: reads(instr)) {
      last_reads[r].insert(node);
    }
    for(auto r: writes(instr)) {
      if(!is_constant(r)){
        last_writes[r] = node;
        last_reads.erase(r);
      }
    }
  };

  for(unsigned i = 0; i != instr_count; i++) {
    auto *instr = &(*std::next(instr_begin, i));
    auto is_long = false;
    auto may_second_slot = false;
    if(enable_dual_issue) {
      auto may_second_slot_extra = std::get<0>(*enable_dual_issue);
      may_second_slot = std::get<1>(*enable_dual_issue)(may_second_slot_extra, instr);
      is_long = std::get<2>(*enable_dual_issue)(instr);
    }
    assert(is_long? !may_second_slot: true);
    std::shared_ptr<Node> node(new Node(i, latency(instr), may_second_slot, is_long, {},{}));

    // Control dependency
    // Must not reorder past conditional branches
    if(last_branch) {
      node->preds[*last_branch] |= true;
    }

    // Memory access dependency
    // Must not change the order of memory accesses
    if(last_mem_acc && memory_access(instr)) {
      node->preds[*last_mem_acc] |= false;
    }
    if(memory_access(instr)) {
      last_mem_acc = node;
    }

    // True dependencies (read must follow last write)
    for(auto read_op : reads(instr)) {
      if(last_writes.count(read_op)) {
    	// Calls don't actually read anything, so they have weak dependency
    	// This doesn't work for indirect calls (which we don't have since we use single-path)
        node->preds[last_writes[read_op]] |= instr->isCall()? false : true;
      }
    }

    // Anti-dependency (write must follow last read)
    for(auto write_op : writes(instr)) {
      if(last_reads.count(write_op)) {
        for(auto read_node: last_reads[write_op]) {
          // Calls read in the called function, which runs after the call instruction
          // executes. So can't let writes following the call be bundled with the call
          // itself, as that would mean the writes execute before the function body.
          node->preds[read_node] |=
        		  std::next(instr_begin, read_node->idx)->isCall()? true : false;
        }
      }
    }

    // Output dependency (write must follow last write)
    for(auto write_op : writes(instr)) {
      if(last_writes.count(write_op)) {
    	// Calls don't write immediately (their bodies do the writing)
	    // meaning we just need to ensure the call doesn't happen before a preceding write
        node->preds[last_writes[write_op]] |=
        		std::next(instr_begin, node->idx)->isCall()? false : true;
      }
    }

    // if branch, it depends on all previous nodes (until previous branch)
    if(conditional_branch(instr)) {
      for(auto prev_node: last_non_branch) {
        node->preds[prev_node] |= false;
      }
      last_non_branch.clear();
      last_branch = node;
    } else {
      last_non_branch.insert(node);
    }

    // Assign node as successor of all predecessors
    for(auto entry: node->preds) {
      entry.first->succs.insert(node);
    }

    if(node->preds.size() == 0) {
      roots.insert(node);
    }

    update_read_writes(node);
  }

  return roots;
}

/// Returns the next node in the ready set that can be scheduled.
///
/// If enable_dual_issue is given, returns the next ready node that may
/// scheduled in the second issue slot
template<
  typename Instruction,
  typename InstrIter,
  typename Operand,
  typename Bundleable
>
Optional<std::shared_ptr<Node>> get_next_ready(
    InstrIter instr_begin, InstrIter instr_end,
    std::set<std::shared_ptr<Node>> &ready,
    std::map<std::shared_ptr<Node>, std::pair<unsigned, std::set<Operand>>> executing,
    std::set<Operand> (*reads)(const Instruction *),
    std::set<Operand> (*writes)(const Instruction *),
    bool enable_second_slot,
    bool requesting_second_slot,
    bool can_be_long,
    Bundleable bundleable
) {
  assert(requesting_second_slot? !can_be_long:true
      && "Cannot request long instruction in second slot");
  LLVM_DEBUG(
    dbgs() << "\nReady set (indices): ";
    for(auto r: ready) {
      dbgs() << r->idx << ", ";
    }
    dbgs() << "\n";
    dbgs() << "Executing set (Index, Cycles, Poison Operands): ";
    for(auto entry: executing) {
      dbgs() << "("<< entry.first->idx << ", " << entry.second.first << ", [";
      for(auto op: entry.second.second) {
        dbgs() << op << ", ";
      }
      dbgs() << "]), ";
    }
    dbgs() << "\n";
  );
  // Get all ready nodes that don't access poisoned operands and can be
  // scheduled in the second issue slot (if needed)
  std::set<std::shared_ptr<Node>> unpoisoned;
  for(auto node: ready){
    auto *instr = &(*std::next(instr_begin, node->idx));
    auto reads_set = reads(instr);
    auto writes_set = writes(instr);

    std::set<Operand> accesses;
    accesses.insert(reads_set.begin(), reads_set.end());
    accesses.insert(writes_set.begin(), writes_set.end());

    if( (requesting_second_slot? node->may_second_slot: true) &&
        bundleable(instr) &&
        (can_be_long? true:!node->is_long) &&
      std::all_of(accesses.begin(), accesses.end(), [&](auto op){
        for(auto entry: executing)  {
          if(entry.second.second.count(op)){
            // Operand is poisoned
      		// For calls, Allow operands with 0 poison cycles left
        	if(!instr->isCall() || entry.second.first != 0) {
                return false;
        	}
          }
        }
        return true;
      })
    ){
      unpoisoned.insert(node);
      // Make sure long instructions aren't scheduled in the second slot
      assert(requesting_second_slot? !node->is_long: true);
      // Make sure long instructions aren't scheduled when disallowed
      assert((!can_be_long)? !node->is_long: true);
    }
  }

  LLVM_DEBUG(
    dbgs() << "Unpoisoned/bundleable nodes (indices): ";
    for(auto node: unpoisoned) {
      dbgs() << node->idx << ", ";
    }
    dbgs() << "\n";
  );

  Optional<std::shared_ptr<Node>> result = None;
  if(unpoisoned.size() != 0) {
    // Choice priority:
    // * Ineligibility for second slot (ineligible instructions first, only if using dual-issue)
    // * Instruction length (long instruction before short, if first slot and only if using dual-issue)
    // * Delay length (long delay before short)
    // * Index (lowest index first)
    auto choice = unpoisoned.begin();
    for(auto unpoisoned_iter = std::next(unpoisoned.begin()); unpoisoned_iter != unpoisoned.end(); unpoisoned_iter++) {
      if(
        (enable_second_slot?!(*unpoisoned_iter)->may_second_slot && (*choice)->may_second_slot:false) ||
        (*unpoisoned_iter)->latency > (*choice)->latency ||
        ((*unpoisoned_iter)->latency == (*choice)->latency && (
           (!requesting_second_slot && (*unpoisoned_iter)->is_long) ||
           (*unpoisoned_iter)->idx < (*choice)->idx
        ))
      ) {
        choice = unpoisoned_iter;
      }
    }
    result = *choice;
    ready.erase(*choice);
  }

  LLVM_DEBUG(
    dbgs() << "Next Chosen Ready (Index): ";
    if(result) {
      dbgs() << (*result)->idx;
    } else {
      dbgs() << "None";
    }
    dbgs() << "\n";
  );

  return result;
}

/// Schedules the given block's instructions.
///
/// Returns a mapping from the new schedule to the original one.
/// I.e. [(0,1), (1,0)] means the first instruction in the new schedule
/// should be the second instruction in the old schedule. Likewise, the second
/// instruction in the new schedule should be the first in the original one.
///
/// If dual-issue is requested, the new schedule indices are divided in 2:
/// Odd indices for the first instructions in each bundle, and even indices
/// for the second instruction. If a bundle doesn't have a second instruction,
/// its even index will not be occupied.
/// E.g. [(0,1), (1,0)] means the first instruction in the first bundle in the new
/// schedule should be the second instruction in the old schedule. The second instruction
/// in the first bundle should then be the first instruction in the old schedule.
///
/// If a target schedule index does not have a mapping, it means that index must be given a Nop.
///
/// Arguments:
/// * mbb: The block to schedule
/// * reads: given an instruction, returns the operands that instruction reads from.
/// * writes: given an instruction, returns the operands that instruction writes to.
/// * poisons: given an instruction, returns whether the write operands are poisoned
///      poisoned until the instruction finishes executing. Poisoned operands can't be used
///      by any other instruction while the poisoning instruction is executing.
///      E.g. Patmos's loads poison the target register in the delay slot.
///      On the other hand, the multiply instruction doesn't poison the sl and sh registers.
/// * memory_access: given an instruction, returns whether either reads or writes to memory.
/// * latency: given an instruction, returns how many cycles above 1 it takes to execute.
///           E.g. Patmos' loads have a latency of 1, while delayed branches have 2.
///           The usual instruction like add have 0
/// * is_constant: given an operand, returns whether that operand is always constant.
///               E.g. Patmos' r0 and p0
/// * conditional_branch: Whether this instruction is a conditional branch our of the block.
///                 E.g. Patmos::BR. Call instructions don't count.
/// * enable_dual_issue: If given, enables dual issue code and contains
///                 1) a function that should return true if the given instruction may be
///                    scheduled in the second issue slot. Otherwise false.
///                 2) a function that should return true if the instruction takes
///                    up both issue slots. Otherwise false.
///                 3) a function that should return true the two given instructions
///                    may be scheduled in the same bundle
///
/// This scheduler can handle conditional branch instructions in the middle of the instruction
/// list however does not attempt to occupy their delay slots nor add Nops.
/// Therefore, post-processing is needed to ensure delayed branches/calls get at least Nops after them.
///
/// Note: Does not account for inter-functional scheduling requirements.
/// 	  E.g. if a blocks ends with high-latency instruction and falls through
///       to a block that uses the poisoned or written-to registers in the first instruction.
template<
  typename Instruction,
  typename InstrIter,
  typename Operand,
  typename MAY_SECOND_SLOT_EXTRA
>
std::map<unsigned, unsigned> list_schedule(
  InstrIter instr_begin, InstrIter instr_end,
  std::set<Operand> (*reads)(const Instruction *),
  std::set<Operand> (*writes)(const Instruction *),
  bool (*poisons)(const Instruction *),
  bool (*memory_access)(const Instruction *),
  unsigned (*latency)(const Instruction *),
  bool (*is_constant)(Operand),
  bool (*conditional_branch)(const Instruction *),
  Optional<std::tuple<
    MAY_SECOND_SLOT_EXTRA,
    bool (*)(MAY_SECOND_SLOT_EXTRA, const Instruction *),
    bool (*)(const Instruction *),
    bool (*)(const Instruction *,const Instruction *)
  >> enable_dual_issue
) {
  std::map<unsigned, unsigned> result;
  if(std::distance(instr_begin, instr_end) == 0) {
    return result;
  }

  auto dependence_roots = dependence_graph(instr_begin, instr_end,
      reads, writes, poisons, memory_access, latency, is_constant, conditional_branch, enable_dual_issue);
  LLVM_DEBUG(Node::dump_graph(dependence_roots, dbgs()));

  // Instructions that have already been scheduled and finished executing
  std::set<std::shared_ptr<Node>> scheduled;

  // Instructions that have been scheduled but haven't finished executing yet.
  // The value is how many more cycles the execution lasts.
  // The set is the operands that are poisoned while the execution lasts
  std::map<std::shared_ptr<Node>, std::pair<unsigned, std::set<Operand>>> executing;
  auto update_executing = [&](){

    // Any executing that only have 0 left need to be moved to scheduled
    for(auto entry: executing){
      if(entry.second.first == 0) {
        scheduled.insert(entry.first);
      }
    }

    // Decrement values
    // First, clear any that have no latency left
    for(auto iter = executing.begin(); iter != executing.end(); ){
      if(iter->second.first == 0) {
        executing.erase(iter);
        iter = executing.begin();
      } else {
        iter++;
      }
    }

    // Then reduce all other's latency by 1
    for(auto iter = executing.begin(); iter != executing.end(); iter++){
      iter->second.first -= 1;
    }
  };

  auto ready_nodes = [&](){
    std::set<std::shared_ptr<Node>> ready;

    // Add all root nodes to potential ready nodes
    ready.insert(dependence_roots.begin(), dependence_roots.end());

    // Add all successors of scheduled nodes
    for(auto node: scheduled) {
      ready.insert(node->succs.begin(), node->succs.end());
    }

    // Add all successors of executing nodes with a weak dependency
    for(auto entry: executing) {
      for(auto succ: entry.first->succs) {
        if(succ->preds.count(entry.first) && !succ->preds[entry.first]) {
          ready.insert(succ);
        }
      }
    }

    // Remove nodes that have already been scheduled
    for(auto node: scheduled) {
      ready.erase(node);
    }

    // Remove nodes that are being executed
    for(auto node: executing) {
      ready.erase(node.first);
    }

    // Remove any node who has a predecessor that hasn't been scheduled (if weak dependency)
    // or hasn't finished executing (if strong dependency)
    for(auto ready_iter = ready.begin(); ready_iter!=ready.end(); ){
      auto node = *ready_iter;
      if(std::any_of(node->preds.begin(), node->preds.end(), [&](auto entry){
        if(entry.second) {
          // Strong dependency, must have finished executing
          return !scheduled.count(entry.first);
        } else {
          // Weak dependency, must be executing or finished
          // Except if this is a call, then executing may have only 0 cycles left
          auto is_call = std::next(instr_begin, node->idx)->isCall();
          return !(
        	(executing.count(entry.first) && (!is_call || executing[entry.first].first==0)) ||
			scheduled.count(entry.first)
		  );
        }

      })){
        ready.erase(node);
        ready_iter = ready.begin();
      } else {
        ready_iter++;
      }
    }
    return ready;
  };

  std::set<std::shared_ptr<Node>> ready = ready_nodes();
  for(unsigned idx = 0; (ready.size() + executing.size()) > 0; idx++, ready = ready_nodes()) {
    auto schedule_instruction = [&](auto next_node, unsigned idx){
      auto *next_instr = &(*std::next(instr_begin, next_node->idx));

      // Schedule instruction
      assert(!result.count(idx) && "Schedule position already assigned");
      result[idx] = next_node->idx;

      std::set<Operand> new_poisons;
      if(poisons(next_instr)) {
        new_poisons = writes(next_instr);
        // Make sure to not include the constant operands in the poison set
        for(auto iter = new_poisons.begin(); iter != new_poisons.end();){
          if(is_constant(*iter)) {
            new_poisons.erase(iter);
            iter = new_poisons.begin();
          } else {
            iter++;
          }
        }
      }
      executing[next_node] = std::make_pair(next_node->latency, new_poisons);
    };

    auto next = get_next_ready(instr_begin, instr_end, ready, executing, reads, writes,
        enable_dual_issue.hasValue(), false, true, [](auto instr){ return true;});
    if(next) {
      schedule_instruction(*next, enable_dual_issue? idx*2: idx);

      if(enable_dual_issue && !(*next)->is_long) {
        ready = ready_nodes();
        auto next2 = get_next_ready(instr_begin, instr_end, ready, executing, reads, writes,
          enable_dual_issue.hasValue(), !(*next)->may_second_slot, false,
          [&](auto instr){
            if(enable_dual_issue) {
              return std::get<3>(*enable_dual_issue)(
                &*(std::next(instr_begin, (*next)->idx)),
                instr
              );
            } else {
              return true;
            }
          }
        );
        if(next2) {
          assert(!(*next2)->is_long);
          assert(!(!(*next)->may_second_slot && !(*next2)->may_second_slot)
              && "Only one instruction in a bundle can occupy the first slot");
          if(!(*next2)->may_second_slot) {
            // Move the first instruction to the second slot
            result[(idx*2)+1] = (*next)->idx;
            result.erase(idx*2);
            // Put the second in the first
            schedule_instruction(*next2, idx*2);
          } else {
            schedule_instruction(*next2, (idx*2)+1);
          }
        }
      }
    }
    update_executing();
  }

  // Various schedule validity checks

  // Check long instruction aren't in second slot
  // Check long instruction aren't bundled with another instruction
  for(auto entry: result) {
	  if(enable_dual_issue) {
		  auto is_long = std::get<2>(*enable_dual_issue)(&*std::next(instr_begin, entry.second));
		  if(entry.first%2 != 0) {
			  assert(!is_long && "Long instruction in second issue slot");
		  } else if(is_long) {
			  assert(!result.count(entry.first+1) && "Long instruction is bundled with another instruction");
		  }
	  }
  }

  return result;
}

}
#endif /* TARGET_PATMOS_SINGLEPATH_SPLISTSCHEDULER_H_ */
