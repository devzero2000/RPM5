#ifndef PROBLEM_HPP
#define PROBLEM_HPP
/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : problem.hpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM problem object for TCL
 
This file defines a new object type for TCL - the RPM problem object.


*****************************************************************************/
#include <tcl.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmdb.h>
#include <rpm/header.h>
#include <rpm/rpmps.h>
#include "cmd_base.hpp"

class RPMPRoblem_Obj
{
// The needed procs and structs to interface to TCL
   struct MyType: public Tcl_ObjType
   {
      MyType(void);
      ~MyType();
   };
public:
   static MyType mytype;
private:

   struct Prob_strings
   {
      const char *name;
      int code;
   };
   enum PARTS
   {
      PACKAGE,ALT,KEY,TYPE,IGNORE,STRING,INT
   };
   static const char *prob_parts[];

   static Prob_strings prob_strings[];
   static inline Tcl_Obj *Part_to_string(PARTS x)
   {
      return Tcl_NewStringObj(prob_parts[x],-1);
   }

   static const char *Prob_to_string(rpmProblemType_e p);
   
   static void FreeInternalRep(Tcl_Obj *objPtr);
   static void DupInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);
   static void UpdateString(Tcl_Obj *objPtr);
   static int  SetFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
   
   mutable int ref_count;
   rpmProblem_s problem;

   void Dup_problem(const rpmProblem_s &x);
   static char *Dup_string(const char *x)
   {
      if (!x)
         return 0;
      int len = strlen(x);
      char *b = new char[len+1];
      strncpy(b,x,len);
      b[len]=0;
      return b;
   }
public:
   static Tcl_Obj *Create_from_problem(const rpmProblem_s &x);
   Tcl_Obj *Get_obj(void);
   RPMPRoblem_Obj(void);
   RPMPRoblem_Obj(const RPMPRoblem_Obj &x);
   RPMPRoblem_Obj(const rpmProblem_s &x);
   ~RPMPRoblem_Obj();
   
   RPMPRoblem_Obj &operator =(const RPMPRoblem_Obj &x);
   
   char *Get_stringrep(int &len);
   
   Tcl_Obj *Get_parts(void); // Return parsed-out parts list
   Tcl_Obj *Get_part(PARTS x); // Return a part of a problem
   
   // Reference counting routines
   int Inc_refcount(void);
   int Dec_refcount(void);
   RPMPRoblem_Obj *Get_exclusive(void);
   RPMPRoblem_Obj *Dup(void) { Inc_refcount(); return this;}
   
   int Problem(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[],Tcl_Obj *prob,int first_tag);
};

#endif

