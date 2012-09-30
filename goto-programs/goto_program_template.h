/*******************************************************************\

Module: Goto Program Template

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#ifndef CPROVER_GOTO_PROGRAM_TEMPLATE_H
#define CPROVER_GOTO_PROGRAM_TEMPLATE_H

#include <irep2.h>
#include <assert.h>
/*! \defgroup gr_goto_programs Goto programs */

#include <cassert>
#include <ostream>
#include <set>

#include <namespace.h>
#include <location.h>

typedef enum { NO_INSTRUCTION_TYPE=0,
               GOTO=1,          // branch, possibly guarded
               ASSUME=2,        // non-failing guarded self loop
               ASSERT=3,        // assertions
               OTHER=4,         // anything else
               SKIP=5,          // just advance the PC
               LOCATION=8,      // semantically like SKIP
               END_FUNCTION=9,  // exit point of a function
               ATOMIC_BEGIN=10, // marks a block without interleavings
               ATOMIC_END=11,   // end of a block without interleavings
               RETURN=12,       // return from a function
               ASSIGN=13,       // assignment lhs:=rhs
               DECL=14,         // declare a local variable
               DEAD=15,         // marks the end-of-live of a local variable
               FUNCTION_CALL=16,// call a function
               THROW=17,        // throw an exception
               CATCH=18         // catch an exception
             }
  goto_program_instruction_typet;

std::ostream &operator<<(std::ostream &, goto_program_instruction_typet);

/*! \brief A generic container class for a control flow graph
           for one function, in the form of a goto-program
    \ingroup gr_goto_programs
*/
template <class codeT, class guardT>
class goto_program_templatet
{
public:
  /*! \brief copy constructor
      \param[in] src an empty goto program
      \remark Use copy_from to copy non-empty goto-programs
  */
  inline goto_program_templatet(const goto_program_templatet &src)
  {
    // DO NOT COPY ME! I HAVE POINTERS IN ME!
    assert(src.instructions.empty());
  }

  /*! \brief assignment operator
      \param[in] src an empty goto program
      \remark Use copy_from to copy non-empty goto-programs
  */
  inline goto_program_templatet &operator=(const goto_program_templatet &src)
  {
    // DO NOT COPY ME! I HAVE POINTERS IN ME!
    assert(src.instructions.empty());
    instructions.clear();
    update();
    return *this;
  }

  // local variables
  typedef std::set<irep_idt> local_variablest;

  /*! \brief Container for an instruction of the goto-program
  */
  class instructiont
  {
  public:
    codeT code;

    //! function this belongs to
    irep_idt function;

    //! the location of the instruction in the source file
    locationt location;

    //! what kind of instruction?
    goto_program_instruction_typet type;

    //! guard for gotos, assume, assert
    guardT guard;

    // for sync
    irep_idt event;

    //! the target for gotos and for start_thread nodes
    typedef typename std::list<class instructiont>::iterator targett;
    typedef typename std::list<class instructiont>::const_iterator const_targett;
    typedef std::list<targett> targetst;
    typedef std::list<const_targett> const_targetst;

    targetst targets;

    //! goto target labels
    typedef std::list<irep_idt> labelst;
    labelst labels;

    std::set<targett> incoming_edges;

    //! is this node a branch target?
    inline bool is_target() const
    { return target_number!=unsigned(-1); }

    //! clear the node
    inline void clear(goto_program_instruction_typet _type)
    {
      type=_type;
      targets.clear();
      guard = true_expr;
      event="";
      code = expr2tc();
    }

    inline void make_goto() { clear(GOTO); }
    inline void make_return() { clear(RETURN); }
    inline void make_function_call(const codeT &_code) { clear(FUNCTION_CALL); code=_code; }
    inline void make_skip() { clear(SKIP); }
    inline void make_throw() { clear(THROW); }
    inline void make_catch() { clear(CATCH); }
    inline void make_assertion(const guardT &g) { clear(ASSERT); guard=g; }
    inline void make_assumption(const guardT &g) { clear(ASSUME); guard=g; }
    inline void make_assignment() { clear(ASSIGN); }
    inline void make_other() { clear(OTHER); }
    inline void make_decl() { clear(DECL); }
    inline void make_dead() { clear(DEAD); }
    inline void make_atomic_begin() { clear(ATOMIC_BEGIN); }
    inline void make_atomic_end() { clear(ATOMIC_END); }

    inline void make_goto(
      typename std::list<class instructiont>::iterator _target)
    {
      make_goto();
      targets.push_back(_target);
    }
    
    inline void make_goto(
      typename std::list<class instructiont>::iterator _target,
      const guardT &g)
    {
      make_goto(_target);
      guard=g;
    }

    // valid local variables at this point
    // this is obsolete and will be removed in future versions
    local_variablest local_variables;

    void add_local_variable(const irep_idt &id)
    {
      local_variables.insert(id);
    }

    void add_local_variables(
      const local_variablest &locals)
    {
      local_variables.insert(locals.begin(), locals.end());
    }

    void add_local_variables(
      const std::list<irep_idt> &locals)
    {
      for(typename std::list<irep_idt>::const_iterator
          it=locals.begin();
          it!=locals.end();
          it++)
        local_variables.insert(*it);
    }

    inline bool is_goto         () const { return type==GOTO;          }
    inline bool is_return       () const { return type==RETURN;        }
    inline bool is_assign       () const { return type==ASSIGN;        }
    inline bool is_function_call() const { return type==FUNCTION_CALL; }
    inline bool is_throw        () const { return type==THROW; }
    inline bool is_catch        () const { return type==CATCH;         }
    inline bool is_skip         () const { return type==SKIP;          }
    inline bool is_location     () const { return type==LOCATION;      }
    inline bool is_other        () const { return type==OTHER;         }
    inline bool is_assume       () const { return type==ASSUME;        }
    inline bool is_assert       () const { return type==ASSERT;        }
    inline bool is_atomic_begin () const { return type==ATOMIC_BEGIN;  }
    inline bool is_atomic_end   () const { return type==ATOMIC_END;    }
    inline bool is_end_function () const { return type==END_FUNCTION;  }

    inline instructiont():
      location(static_cast<const locationt &>(get_nil_irep())),
      type(NO_INSTRUCTION_TYPE),
      location_number(0),
      target_number(unsigned(-1))
    {
      guard = true_expr;
    }

    inline instructiont(goto_program_instruction_typet _type):
      location(static_cast<const locationt &>(get_nil_irep())),
      type(_type),
      location_number(0),
      target_number(unsigned(-1))
    {
      guard = true_expr;
    }

    //! swap two instructions
    void swap(instructiont &instruction)
    {
      instruction.code.swap(code);
      instruction.location.swap(location);
      std::swap(instruction.type, type);
      instruction.guard.swap(guard);
      instruction.event.swap(event);
      instruction.targets.swap(targets);
      instruction.local_variables.swap(local_variables);
      instruction.function.swap(function);
    }

    //! A globally unique number to identify a program location.
    //! It's guaranteed to be ordered in program order within
    //! one goto_program.
    unsigned location_number;

    //! Number unique per function to identify loops
    unsigned loop_number;

    //! A number to identify branch targets.
    //! This is -1 if it's not a target.
    unsigned target_number;

    //! Returns true if the instruction is a backwards branch.
    bool is_backwards_goto() const
    {
      if(!is_goto()) return false;

      for(typename targetst::const_iterator
          it=targets.begin();
          it!=targets.end();
          it++)
        if((*it)->location_number<=location_number)
          return true;

      return false;
    }

    bool operator<(const class instructiont i1) const
    {
      if (function < i1.function)
        return true;

      if (location_number < i1.location_number)
        return true;

      return false;
    }
  };

  typedef std::list<class instructiont> instructionst;

  typedef typename instructionst::iterator targett;
  typedef typename instructionst::const_iterator const_targett;
  typedef typename std::list<targett> targetst;
  typedef typename std::list<const_targett> const_targetst;

  //! The list of instructions in the goto program
  instructionst instructions;

  bool has_local_variable(
    class instructiont &instruction,
    const irep_idt &identifier)
  {
    return instruction.local_variables.count(identifier)!=0;
  }

  void get_successors(
    targett target,
    targetst &successors);

  void get_successors(
    const_targett target,
    const_targetst &successors) const;

  void compute_incoming_edges();

  //! Insertion that preserves jumps to "target".
  //! The instruction is destroyed.
  void insert_swap(targett target, instructiont &instruction)
  {
    assert(target!=instructions.end());
    targett next=target;
    next++;
    instructions.insert(next, instructiont())->swap(*target);
    target->swap(instruction);
  }

  //! Insertion that preserves jumps to "target".
  //! The program p is destroyed.
  void insert_swap(targett target, goto_program_templatet<codeT, guardT> &p)
  {
    assert(target!=instructions.end());
    if(p.instructions.empty()) return;
    insert_swap(target, p.instructions.front());
    targett next=target;
    next++;
    p.instructions.erase(p.instructions.begin());
    instructions.splice(next, p.instructions);
  }

  //! Insertion before the given target
  //! \return newly inserted location
  inline targett insert(targett target)
  {
    return instructions.insert(target, instructiont());
  }

  //! Appends the given program, which is destroyed
  inline void destructive_append(goto_program_templatet<codeT, guardT> &p)
  {
    instructions.splice(instructions.end(),
                        p.instructions);
  }

  //! Inserts the given program at the given location.
  //! The program is destroyed.
  inline void destructive_insert(
    targett target,
    goto_program_templatet<codeT, guardT> &p)
  {
    instructions.splice(target,
                        p.instructions);
  }

  //! Adds an instruction at the end.
  //! \return The newly added instruction.
  inline targett add_instruction()
  {
    instructions.push_back(instructiont());
    targett tmp = instructions.end();
    return --tmp;
  }

  //! Adds an instruction of given type at the end.
  //! \return The newly added instruction.
  inline targett add_instruction(goto_program_instruction_typet type)
  {
    instructions.push_back(instructiont(type));
    targett tmp = instructions.end();
    return --tmp;
  }

  //! Output goto program to given stream
  std::ostream &output(
    const namespacet &ns,
    const irep_idt &identifier,
    std::ostream &out) const;

  //! Output goto-program to given stream
  inline std::ostream &output(std::ostream &out) const
  {
    return output(namespacet(contextt()), "", out);
  }

  //! Output a single instruction
  virtual std::ostream &output_instruction(
    const namespacet &ns,
    const irep_idt &identifier,
    std::ostream &out,
    typename instructionst::const_iterator it,
    bool show_location = true, bool show_variables = false) const=0;

  //! Compute the target numbers
  void compute_target_numbers();

  //! Compute location numbers
  void compute_location_numbers(unsigned &nr)
  {
    for(typename instructionst::iterator
        it=instructions.begin();
        it!=instructions.end();
        it++)
      it->location_number=nr++;
  }

  //! Compute location numbers
  inline void compute_location_numbers()
  {
    unsigned nr=0;
    compute_location_numbers(nr);
  }

  //! Compute loop numbers
  void compute_loop_numbers(unsigned int &num);

  //! Update all indices
  void update();

  //! Is the program empty?
  inline bool empty() const
  {
    return instructions.empty();
  }

  //! Constructor
  goto_program_templatet()
  {
  }

  virtual ~goto_program_templatet()
  {
  }

  //! Swap the goto program
  inline void swap(goto_program_templatet<codeT, guardT> &program)
  {
    program.instructions.swap(instructions);
  }

  //! Clear the goto program
  inline void clear()
  {
    instructions.clear();
  }

  //! Copy a full goto program, preserving targets
  void copy_from(const goto_program_templatet<codeT, guardT> &src);

  //! Does the goto program have an assertion?
  bool has_assertion() const;
};

template <class codeT, class guardT>
void goto_program_templatet<codeT, guardT>::compute_loop_numbers
                                            (unsigned int &num)
{
  for(typename instructionst::iterator
      it=instructions.begin();
      it!=instructions.end();
      it++)
    if(it->is_backwards_goto())
      it->loop_number=num++;
}

template <class codeT, class guardT>
void goto_program_templatet<codeT, guardT>::get_successors(
  targett target,
  targetst &successors)
{
  successors.clear();
  if(target==instructions.end()) return;

  targett next=target;
  next++;

  const instructiont &i=*target;

  if(i.is_goto())
  {
    for(typename targetst::const_iterator
        t_it=i.targets.begin();
        t_it!=i.targets.end();
        t_it++)
      successors.push_back(*t_it);

    if(!is_true(i.guard))
      successors.push_back(next);
  }
  else if(i.is_throw())
  {
    // the successors are non-obvious
  }
  else if(i.is_return())
  {
    // the successor is the end_function at the end of the function
    successors.push_back(--instructions.end());
  }
  else if(i.is_assume())
  {
    if(!is_false(i.guard))
      successors.push_back(next);
  }
  else
    successors.push_back(next);
}

template <class codeT, class guardT>
void goto_program_templatet<codeT, guardT>::get_successors(
  const_targett target,
  const_targetst &successors) const
{
  successors.clear();
  if(target==instructions.end()) return;

  const_targett next=target;
  next++;

  const instructiont &i=*target;

  if(i.is_goto())
  {
    for(typename targetst::const_iterator
        t_it=i.targets.begin();
        t_it!=i.targets.end();
        t_it++)
      successors.push_back(*t_it);

    if(!is_true(i.guard))
      successors.push_back(next);
  }
  else if(i.is_return())
  {
    // the successor is the end_function at the end
    successors.push_back(--instructions.end());
  }
  else if(i.is_assume())
  {
    if(!is_false(i.guard))
      successors.push_back(next);
  }
  else
    successors.push_back(next);
}

#include <langapi/language_util.h>
#include <iomanip>

template <class codeT, class guardT>
void goto_program_templatet<codeT, guardT>::update()
{
  compute_incoming_edges();
  compute_target_numbers();
  compute_location_numbers();
}

template <class codeT, class guardT>
std::ostream& goto_program_templatet<codeT, guardT>::output(
  const namespacet &ns,
  const irep_idt &identifier,
  std::ostream& out) const
{
  // output program

  for(typename instructionst::const_iterator
      it=instructions.begin();
      it!=instructions.end();
      it++)
    output_instruction(ns, identifier, out, it);

  return out;
}

template <class codeT, class guardT>
void goto_program_templatet<codeT, guardT>::compute_target_numbers()
{
  // reset marking

  for(typename instructionst::iterator
      it=instructions.begin();
      it!=instructions.end();
      it++)
    it->target_number=-1;

  // mark the goto targets

  for(typename instructionst::const_iterator
      it=instructions.begin();
      it!=instructions.end();
      it++)
  {
    for(typename instructiont::targetst::const_iterator
        t_it=it->targets.begin();
        t_it!=it->targets.end();
        t_it++)
    {
      targett t=*t_it;
      if(t!=instructions.end())
        t->target_number=0;
    }
  }

  // number the targets properly
  unsigned cnt=0;

  for(typename instructionst::iterator
      it=instructions.begin();
      it!=instructions.end();
      it++)
  {
    if(it->is_target())
    {
      it->target_number=++cnt;
      assert(it->target_number!=0);
    }
  }

  // check the targets!
  // (this is a consistency check only)

  for(typename instructionst::const_iterator
      it=instructions.begin();
      it!=instructions.end();
      it++)
  {
    for(typename instructiont::targetst::const_iterator
        t_it=it->targets.begin();
        t_it!=it->targets.end();
        t_it++)
    {
      targett t=*t_it;
      if(t!=instructions.end())
      {
        assert(t->target_number!=0);
        assert(t->target_number!=unsigned(-1));
      }
    }
  }

}

template <class codeT, class guardT>
void goto_program_templatet<codeT, guardT>::copy_from(
  const goto_program_templatet<codeT, guardT> &src)
{
  // Definitions for mapping between the two programs
  typedef std::map<const_targett, targett> targets_mappingt;
  targets_mappingt targets_mapping;

  clear();

  // Loop over program - 1st time collects targets and copy

  for(typename instructionst::const_iterator
      it=src.instructions.begin();
      it!=src.instructions.end();
      it++)
  {
    targett new_instruction=add_instruction();
    targets_mapping[it]=new_instruction;
    *new_instruction=*it;
  }

  // Loop over program - 2nd time updates targets

  for(typename instructionst::iterator
      it=instructions.begin();
      it!=instructions.end();
      it++)
  {
    for(typename instructiont::targetst::iterator
        t_it=it->targets.begin();
        t_it!=it->targets.end();
        t_it++)
    {
      typename targets_mappingt::iterator
        m_target_it=targets_mapping.find(*t_it);

      if(m_target_it==targets_mapping.end())
        throw "copy_from: target not found";

      *t_it=m_target_it->second;
    }
  }

  compute_incoming_edges();
  compute_target_numbers();
}

// number them
template <class codeT, class guardT>
bool goto_program_templatet<codeT, guardT>::has_assertion() const
{
  for(typename instructionst::const_iterator
      it=instructions.begin();
      it!=instructions.end();
      it++)
    if(it->is_assert() && !it->guard.is_true())
      return true;

  return false;
}

template <class codeT, class guardT>
void goto_program_templatet<codeT, guardT>::compute_incoming_edges()
{
  for(typename instructionst::iterator
      it=instructions.begin();
      it!=instructions.end();
      it++)
  {
    it->incoming_edges.clear();
  }

  for(typename instructionst::iterator
      it=instructions.begin();
      it!=instructions.end();
      it++)
  {
    targetst successors;

    get_successors(it, successors);

    for(typename targetst::const_iterator
        s_it=successors.begin();
        s_it!=successors.end();
        s_it++)
    {
      targett t=*s_it;

      if(t!=instructions.end())
        t->incoming_edges.insert(it);
    }
  }
}

template <class codeT, class guardT>
bool operator<(const typename goto_program_templatet<codeT, guardT>::const_targett i1,
               const typename goto_program_templatet<codeT, guardT>::const_targett i2);

#endif
