#ifndef DEPENDENCY_HPP
#define DEPENDENCY_HPP
/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : dependency.hpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM RPMDependency object for TCL
 
This file defines a new object type for TCL - the RPMDependency object.


*****************************************************************************/
#include <tcl.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmdb.h>
#include <rpm/header.h>
#include <rpm/rpmps.h>
#include "cmd_base.hpp"

class RPMDependency_Obj
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

   static void FreeInternalRep(Tcl_Obj *objPtr);
   static void DupInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);
   static void UpdateString(Tcl_Obj *objPtr);
   static int  SetFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
   
   mutable int ref_count;
   rpmds dependency;

   void Dup_dependency(const rpmds x);

public:
   static Tcl_Obj *Create_from_ds(const rpmds x);
   Tcl_Obj *Get_obj(void);
   RPMDependency_Obj(void);
   RPMDependency_Obj(const RPMDependency_Obj &x);
   RPMDependency_Obj(const rpmds x);
   ~RPMDependency_Obj();
   
   RPMDependency_Obj &operator =(const RPMDependency_Obj &x);
   
   char *Get_stringrep(int &len);
   
   // Reference counting routines
   int Inc_refcount(void);
   int Dec_refcount(void);
   RPMDependency_Obj *Get_exclusive(void);
   RPMDependency_Obj *Dup(void) { Inc_refcount(); return this;}

};

#endif

