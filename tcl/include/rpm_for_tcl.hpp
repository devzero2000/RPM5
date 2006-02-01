#ifndef RPM_FOR_TCL_HPP
#define RPM_FOR_TCL_HPP
/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : rpm_for_tcl.hpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM bindings for TCL
 
This file implements an verb for the TCL language to allow manipulating
RedHat Package Manager (RPM) files and databases.


*****************************************************************************/
#include <tcl.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmdb.h>
#include <rpm/header.h>
#include <rpm/rpmlog.h>
#include "cmd_base.hpp"
#include "header_obj.hpp"
#include "transaction_set.hpp"
#include "problem.hpp"


extern "C" int Rpm_Init(Tcl_Interp *interp);

class RPM_for_TCL: public Cmd_base
{
   struct Tag_list;
   typedef int (RPM_for_TCL::*CMD_FUNC)(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   struct Cmd_list
   {
      Cmd_list_base base;
      CMD_FUNC func;
   };
   static const Cmd_list cmds[];
   static const Cmd_list Cache_cmds[];
        
   struct RPMVerbosity;
   static RPMVerbosity verbosities[];
       
   static const Tag_list *tags;
   static const Tag_list extra_tags[];
   static const char version[];
   static const char build_date[];

   rpmdb database;
   rpmdb cache_db;
public:
   static int db_dirtied_count; // Incremented when anybody dirties the database
   static void DBs_are_dirty(void);
   static int Are_DBs_dirty(int &my_count);
      
private:
   int transaction_counter; // Used to auto-gen transaction names
   int my_dirty_count; // used to detect when our in-memory copies need to be updated
   
   friend int Rpm_Init(Tcl_Interp *interp);
public:
   RPM_for_TCL(Tcl_Interp *interp);
   ~RPM_for_TCL();
   
   // base class overrides
   int Help(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Cmd_hook(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Exec(int which,Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
 private:
   void Close_db(void);
   int Open_db(void);
  
public:
   int FindPackage_core(Tcl_Obj *&result,Tcl_Obj *name,rpmTag tag,rpmdb database,Tcl_Interp *_interp);
   int FindPackage_cmd(rpmdb db,int first_arg,Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
private:
   int File_core(Tcl_Obj *&result,Tcl_Obj *name);
         
   // Subcommands
   int Version(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int OS_ver(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Arch_ver(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Close_db(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   
   int IncreaseVerbosity(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int DecreaseVerbosity(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Verbosity(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   
   int FindPackage(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int File(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);

   int HeaderCmd(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Header_core(Tcl_Interp *interp,Tcl_Obj *header,Tcl_Obj *tag);
   int Info(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Tags(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int TagType(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Compare(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
      
   // Defined in transaction_set.hpp
   int Create_transaction(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   
   // Defined in problem.cpp
   int NewProblem(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   
   // Macro (rpmrc) file manipulation
   int Macro(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Expand(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   
   // Cache functions
   int Cache(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Cache_Help(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Cache_Add(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Cache_File(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Cache_Search(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Cache_Init(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Cache_Remove(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   int Cache_Rebuild(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
};

#endif

