#ifndef TRANSACTION_SET_HPP
#define TRANSACTION_SET_HPP
/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : transaction_set.hpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM header object for TCL
 
This file defines a new object type for TCL - the RPM transaction set object.


*****************************************************************************/
#include <tcl.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmdb.h>
#include <rpm/header.h>
#include <rpm/rpmts.h>
#include "cmd_base.hpp"

// A transaction set is modeled as a TCL verb dynamically created.
class RPMTransaction_Set: public Cmd_base
{
   typedef int (RPMTransaction_Set::*CMD_FUNC)(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   struct Cmd_list
   {
      Cmd_list_base base;
      CMD_FUNC func;
   };
   struct Bits
   {
      unsigned bit;
      const char *msg;
   };
   
   static Bits bits[];
   static Bits Prob_bits[];
   static const Cmd_list cmds[];

   rpmts transaction;
   Tcl_Obj    *header_list; // List of header objects we have assigned to the transaction
                            // set, as the ts itself gives us no notice that it has freed them
   int my_dirty_count;      // used to detect database changes
   unsigned prob_flags;
   
   RPMTransaction_Set(Tcl_Interp *interp,const char *name,rpmts ts);

public:
   static RPMTransaction_Set *Create(Tcl_Interp *interp,const char *name);
   static RPMTransaction_Set *Create(Tcl_Interp *interp,Tcl_Obj *obj)
   {
      return Create(interp,Tcl_GetStringFromObj(obj,0));
   }
   ~RPMTransaction_Set();
   int Exec(int which,Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   
   // Callbacks from RPM library
   static void *RPM_callback_entry( const void * h,
                                   const rpmCallbackType what,
                                   const unsigned long amount,
                                   const unsigned long total,
                                   fnpyKey key,
                                   rpmCallbackData data);
   static int Solve(rpmts ts, rpmds key, const void *data);
   
   void *RPM_callback( const void * h, const rpmCallbackType what,
                      const unsigned long amount,
                      const unsigned long total,
                      fnpyKey key);
   int Solve(rpmds key);
   
   // Header data manipulation
   operator rpmts(void) { return transaction;}
   
   // Core routines for commands
   enum Install_mode {INSTALL,UPGRADE,REMOVE};
   int Install_or_remove(Tcl_Obj *item,Install_mode mode);
   Tcl_Obj *Show(Tcl_Obj *item);
   
   // Commands
   int Destroy(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Install(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Upgrade(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Remove(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);

   int Show(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   
   int Check(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Clear(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int ProbFlags(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Order(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Test(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Run(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Problems(Tcl_Interp *interp,int objc=0, Tcl_Obj *const objv[]=0);
};

#endif

