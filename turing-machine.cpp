// 
// C++ templates are Turing-complete. The obvious way to show that is to create
// a system that can execute any Turing machine. That's what's going on here.
// For best results, compile this on a computer with infinite memory, otherwise
// I cannot guarantee the availabality of an infinite tape at all times.
//
// Compile with
//   $CXX -Wall -Wextra -std=c++11 -pedantic turing-machine.cpp -o turing-machine
// I've tested this with $CXX = g++ 4.8.1 and clang++ 3.3.
//

#include <iostream>
#include <typeinfo>
#include <type_traits>
#include <utility>

// 
// Tape: Our tape will be made of two stacks and a char. The two stacks will
// hold the portion of the tape to the left or right of the head position.
// Moving the head to one side or the other will be made by simply moving
// stuff between the two stacks.
//
// The tape can be printed to std::cout for demonstration purposes. In the
// printed output, each cell will be printed as a separate character with the
// cell currently under the head enclosed in braces. The machine can create new
// tape cells simply by moving onto them -- in that case, they will not be 
// removed when the machine moves back even if the machine does not write
// anything to these empty cells. Empty cells are shown by the '#' character.
//

// The special empty symbol
constexpr char empty = '#';

// Match any symbol or don't change the current symbol
constexpr char wildcard = '?';

template <char Head, typename Tail>
struct stack {
  static constexpr char top = Head;
  using tail = Tail;
};

// We want infinite tape, so we'll use infinite stacks -- nil is a prefectly
// usable cell of the stack that contains the empty symbol -- this way we
// have an infinite source of empty symbols.
struct nil { 
  static constexpr char top = empty;
  using tail = nil;
};

template <char New, typename Stack>
struct push {
  using type = stack<New, Stack>;
};

template <typename Stack>
struct pop {
  using type = typename Stack::tail;
};

// Print stack contents to std::cout delimited by spaces. Does not print a
// newline.
template <typename Stack>
struct print_stack;

template <char Head, typename Tail>
struct print_stack<stack<Head, Tail>> {
  static void
  do_(bool space = false) {
    if (space) std::cout << ' ';
    std::cout << Head;
    print_stack<Tail>::do_(true);
  }
};

template <>
struct print_stack<nil> {
  static void
  do_(bool space = false) {
    if (space) std::cout << ' ';
  }
};

// Also prints a stack, but in reverse order.
template <typename Stack>
struct print_stack_reverse;

template <char Head, typename Tail>
struct print_stack_reverse<stack<Head, Tail>> {
  static void
  do_() {
    print_stack_reverse<Tail>::do_();
    std::cout << Head;
    std::cout << ' ';
  }
};

template <>
struct print_stack_reverse<nil> {
  static void
  do_() { }
};

template <typename Left, char Head, typename Right>
struct tape {
  using left = Left;
  using right = Right;
  static constexpr char head = Head;
};

template <typename Tape>
struct move_left {
  using type = tape<
    typename pop<typename Tape::left>::type,
    Tape::left::top,
    typename push<Tape::head, typename Tape::right>::type
  >;
};

template <typename Tape>
struct move_right {
  using type = tape<
    typename push<Tape::head, typename Tape::left>::type,
    Tape::right::top,
    typename pop<typename Tape::right>::type
  >;
};

namespace detail {
  template <char...> struct stackify;

  template <char C, char... Cs>
  struct stackify<C, Cs...> {
    using type = stack<C, typename stackify<Cs...>::type>;
  };

  template <>
  struct stackify<> {
    using type = nil;
  };
}

// Make a tape out of a list of symbols. It would be awesome if we could make
// it out of a, you know, an actual *string*, but that won't fly. At least not
// in C++11.
//
// The tape is made by putting the first symbol under the head, and the 
// remaining symbols to the right of it, in order.
template <char...> struct make_tape;

template <char First, char... Rest>
struct make_tape<First, Rest...> {
  using type = tape<nil, First, typename detail::stackify<Rest...>::type>;
};

template <>
struct make_tape<> {
  using type = tape<nil, '#', nil>;
};

template <typename Tape>
struct print_tape {
  static void
  do_() {
    print_stack_reverse<typename Tape::left>::do_();
    std::cout << "[" << Tape::head << "]";
    print_stack<typename Tape::right>::do_(true);
    std::cout << '\n';
  }
};

// Helper that will translate 'L', 'R', '0' characters to commands to move
// the head respectively to the left, right or not move it at all.
template <char Dir, typename Tape>
struct move_tape;

template <typename Tape>
struct move_tape<'L', Tape> {
  using type = typename move_left<Tape>::type;
};

template <typename Tape>
struct move_tape<'R', Tape> {
  using type = typename move_right<Tape>::type;
};

template <typename Tape>
struct move_tape<'0', Tape> {
  using type = Tape;
};

// Put a symbol onto the tape under the current head position. If the symbol
// is wildcard ('?'), then the tape is not changed at all.
template <typename Tape, char C>
struct write_tape {
  using type = tape<typename Tape::left, C, typename Tape::right>;
};

template <typename Tape>
struct write_tape<Tape, wildcard> {
  using type = Tape;
};

//
// Instructions: We will use simply a list of 5-tuples encoding the transitions
// of the machine. When a step is to be made, the list of instructions will be 
// traversed in order and each instruction will be matched against the current
// machine state: the symbol under the head and the state. The first matching
// instruction, if any, will be returned and used to advance the machine's
// progress. If no instruction is found, the machine will simply terminate.
// 
// An instruction may specify wildcard ('?') as the requested read symbol. In
// that case, the instruction will always match the symbol and only the current
// state will be considered. Together with the linear traversal of instruction
// list, this means that instructions using a wildcard should be put after any
// instruction for the same state using a non-wildcard read symbol -- otherwise
// the non-wildcard instruction for the same state won't even be considered.
//

template <
  typename FromState, // The TM state to match
  char HeadRead,      // The symbol under the head to match
  typename ToState,   // The TM state to transition to
  char HeadWrite,     // The symbol to write (or wildcard not to write anything)
  char Movement       // Direction of the head movement
> 
struct instruction {
  using from_state = FromState;
  static constexpr char head_read = HeadRead;
  using to_state = ToState;
  static constexpr char head_write = HeadWrite;
  static constexpr char movement = Movement;
};

// Execute an instruction on the tape. This will write a symbol to the current
// head position and move the tape in the proper direction, but obviously will
// not change the machine's state. It does not care whether the instruction
// matches either, it simply assumes it does.
template <typename Instruction, typename Tape>
struct execute_instruction {
  using state = typename Instruction::to_state;
  using tape = 
    typename move_tape<
      Instruction::movement,
      typename write_tape<Tape, Instruction::head_write>::type
    >::type;
};

// Special "couldn't find any matching instruction" value.
struct fail {};  

namespace detail {
  // Find a matching instruction. 
  // match_helper<CurrentState, CurrentHead, InstructionList>::match will
  // be either the first matching instruction or fail if no instruction
  // matches.
  template <typename, char, typename...>
  struct match_helper;

  template <
    typename State, char Head, typename First, typename... Rest
  >
  struct match_helper<State, Head, First, Rest...> {
    using match =
      typename std::conditional<
        std::is_same<State, typename First::from_state>::value &&
          (Head == First::head_read || First::head_read == wildcard),
        First,
        typename match_helper<State, Head, Rest...>::match
      >::type;
  };

  template <typename State, char Head>
  struct match_helper<State, Head> {
    using match = fail;
  };

}  // end namespace detail

// Instruction list.
template <typename... Instructions>
struct program {
  template <typename State, char Head>
  using match_instruction = 
    typename detail::match_helper<State, Head, Instructions...>::match;
};

//
// The Turing Machine: Executing one step of the machine consists of fetching
// a matching instruction, modifying the tape accordingly, and creating a new
// Turing Machine with the new stack and in the state specified by the
// instruction. The new machine is then subject to the same process.
//
// The process ends when no instruction can be matched or when the matching
// instruction puts the machine into a final state. In either case, the last
// machine thus created is returned as the result.
//

template <typename State, typename Tape, typename Program>
struct machine {
  using state = State;
  using tape = Tape;
  using program = Program;
};

// Shall we continue our glorious journey across the state-space?
template <typename Machine, typename NextInstruction>
struct cont {
  static constexpr bool value = 
    !Machine::state::final && !std::is_same<NextInstruction, fail>::value;
};

// Run the machine until it halts (if it ever does). run<M>::result will be the
// final machine configuration.
template <typename Machine>
struct run;

namespace detail {
  template <typename Machine, typename NextInstruction, bool Continue>
  struct run_helper;

  template <typename Machine, typename NextInstruction>
  struct run_helper<Machine, NextInstruction, true> {
  private:
    using exec = 
      execute_instruction<
        NextInstruction,
        typename Machine::tape
      >;

  public:
    using result = typename run<
      machine<
        typename exec::state, typename exec::tape, typename Machine::program
      >
    >::result;
  };

  template <typename Machine, typename NextInstruction>
  struct run_helper<Machine, NextInstruction, false> {
    using result = Machine;
  };
}  // end namespace detail

template <typename Machine>
struct run {
private:
  using next =
    typename Machine::program::template match_instruction<
      typename Machine::state, Machine::tape::head
    >;

public:
  using result = typename detail::run_helper<
    Machine, next, cont<Machine, next>::value
  >::result;
};

// 
// Actually using our magnificient creation:
//

// Helper type to define states.
template <bool Final = false>
struct state {
  static constexpr bool final = Final;
};

template <typename Machine>
struct print_result {
  static void
  do_() {
    if (Machine::state::final)
      std::cout << "Input accepted.\n";
    else
      std::cout << "Input not accepted.\n";

    std::cout << "Machine halted in state " 
              << typeid(typename Machine::state).name()
              << '\n';
    std::cout << "Final tape configuration:\n";
    print_tape<typename Machine::tape>::do_();
  }
};

template <typename Machine>
struct execute {
  static void
  do_() {
    std::cout << "-------------\n";
    std::cout << "Initial tape:\n";
    print_tape<typename Machine::tape>::do_();
    print_result<typename run<Machine>::result>::do_();
  }
};

int main() {
  std::cout << "Input reversal machine:\n";
  std::cout << "=======================\n";

  struct put_right_marker : state<> { };
  struct rewind : state<> { };
  struct go_right_a : state<> { };
  struct go_right_b : state<> { };
  struct go_left_a : state<> { };
  struct go_left_b : state<> { };
  struct take_left : state<> { };
  struct take_right : state<> { };
  struct clear : state<> { };
  struct clear_a : state<> { };
  struct clear_b : state<> { };
  struct clear_last : state<> { };
  struct end : state<true> { };

  using reverse_prog = program<
    instruction<put_right_marker,   '#', rewind,            '|', 'L'>,
    instruction<put_right_marker,   '?', put_right_marker,  '?', 'R'>,

    instruction<rewind,             '#', take_left,         '#', 'R'>,
    instruction<rewind,             '?', rewind,            '?', 'L'>,

    instruction<take_left,          'a', go_right_a,        '|', 'R'>,
    instruction<take_left,          'b', go_right_b,        '|', 'R'>,
    instruction<take_left,          '|', clear,             '|', 'R'>,
    
    instruction<go_right_a,         '|', take_right,        'a', 'L'>,
    instruction<go_right_b,         '|', take_right,        'b', 'L'>,
    instruction<go_right_a,         '?', go_right_a,        '?', 'R'>,
    instruction<go_right_b,         '?', go_right_b,        '?', 'R'>,

    instruction<take_right,         'a', go_left_a,         '|', 'L'>,
    instruction<take_right,         'b', go_left_b,         '|', 'L'>,
    instruction<take_right,         '|', clear,             '|', 'R'>,

    instruction<go_left_a,          '|', take_left,         'a', 'R'>,
    instruction<go_left_b,          '|', take_left,         'b', 'R'>,
    instruction<go_left_a,          '?', go_left_a,         '?', 'L'>,
    instruction<go_left_b,          '?', go_left_b,         '?', 'L'>,
    
    instruction<clear,              'a', clear_a,           '|', 'L'>,
    instruction<clear,              'b', clear_b,           '|', 'L'>,
    instruction<clear,              '|', clear,             '|', 'R'>,
    instruction<clear,              '#', clear_last,        '#', 'L'>,
    instruction<clear_a,            '|', clear,             'a', 'R'>,
    instruction<clear_b,            '|', clear,             'b', 'R'>,
    instruction<clear_last,         '|', end,               '#', '0'>
  >;

  using reverse_tape1 = make_tape<'a', 'b', 'a', 'a', 'b', 'b', 'a'>::type;
  using reverse_tape2 = make_tape<'a'>::type;
  using reverse_tape3 = make_tape<'a', 'b'>::type;
  using reverse_tape4 = make_tape<>::type;

  execute<machine<put_right_marker, reverse_tape1, reverse_prog>>::do_();
  execute<machine<put_right_marker, reverse_tape2, reverse_prog>>::do_();
  execute<machine<put_right_marker, reverse_tape3, reverse_prog>>::do_();
  execute<machine<put_right_marker, reverse_tape4, reverse_prog>>::do_();

  std::cout << "\n";
  std::cout << "Acceptor of { a^n b^n c^n : n >= 0 }:\n";
  std::cout << "=====================================\n";

  struct check_a  : state<> { };
  struct check_b  : state<> { };
  struct check_c  : state<> { };
  struct find_end : state<> { };
  struct accept   : state<true> { };
  struct fail     : state<> { };
  using lang_prog = program<
    instruction<check_a,    'x', check_a,   'x', 'R'>,
    instruction<check_a,    'a', check_b,   'x', 'R'>,
    instruction<check_a,    '#', accept,    '#', '0'>,
    instruction<check_b,    'b', check_c,   'x', 'R'>,
    instruction<check_b,    '#', fail,      '#', '0'>,
    instruction<check_b,    '?', check_b,   '?', 'R'>,
    instruction<check_c,    'c', find_end,  'x', 'R'>,
    instruction<check_c,    '#', fail,      '#', '0'>,
    instruction<check_c,    '?', check_c,   '?', 'R'>,
    instruction<find_end,   'c', find_end,  'c', 'R'>,
    instruction<find_end,   '#', rewind,    '#', 'L'>,
    instruction<rewind,     '#', check_a,   '#', 'R'>,
    instruction<rewind,     '?', rewind,    '?', 'L'>
  >;

  using lang_tape1 = make_tape<
    'a', 'a', 'a', 'b', 'b', 'b', 'c', 'c', 'c'>::type;
  using lang_tape2 = make_tape<'a', 'b', 'c'>::type;
  using lang_tape3 = make_tape<>::type;
  using lang_tape4 = make_tape<'a', 'a', 'b', 'c', 'c'>::type;
  using lang_tape5 = make_tape<'a', 'a', 'b', 'b', 'c', 'c', 'c'>::type;
  using lang_tape6 = make_tape<'a', 'a', 'b', 'b', 'c'>::type;
  using lang_tape7 = make_tape<'a', 'b', 'c', 'a', 'b', 'c'>::type;
  
  execute<machine<check_a, lang_tape1, lang_prog>>::do_();
  execute<machine<check_a, lang_tape2, lang_prog>>::do_();
  execute<machine<check_a, lang_tape3, lang_prog>>::do_();
  execute<machine<check_a, lang_tape4, lang_prog>>::do_();
  execute<machine<check_a, lang_tape5, lang_prog>>::do_();
  execute<machine<check_a, lang_tape6, lang_prog>>::do_();
  execute<machine<check_a, lang_tape7, lang_prog>>::do_();
}

