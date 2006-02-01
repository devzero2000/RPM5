/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : problem.cpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM problem for TCL
 
This file implements an object type for TCL that contains an RPM problem report

*****************************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "rpm_for_tcl.hpp"
#include <rpm/rpmps.h>

// C-style "vftable" for this class - gosh, it would be soo much easier were TCL
// written in C++....

RPMPRoblem_Obj::MyType RPMPRoblem_Obj::mytype;
RPMPRoblem_Obj::Prob_strings RPMPRoblem_Obj::prob_strings[] =
{
   {"Not for this arch",RPMPROB_BADARCH},
   {"Not for this OS",RPMPROB_BADOS},
   {"Already installed",RPMPROB_PKG_INSTALLED},
   {"Cannot relocate",RPMPROB_BADRELOCATE},
   {"Missing requires",RPMPROB_REQUIRES},
   {"Conflicts",RPMPROB_CONFLICT},
   {"New file conflict",RPMPROB_NEW_FILE_CONFLICT},
   {"file conflict",RPMPROB_FILE_CONFLICT},
   {"Older version",RPMPROB_OLDPACKAGE},
   {"Not enough space",RPMPROB_DISKSPACE},
   {"Not enough inodes",RPMPROB_DISKNODES},
   {"RPMPROB_BADPRETRANS",RPMPROB_BADPRETRANS},
   {0,0}
};

// names of the parts - this MUST MATCH THE RPMPRoblem_Obj::PARTS enum!
const char *RPMPRoblem_Obj::prob_parts[] =
{
   "-package",
   "-alt",
   "-key",
   "-type",
   "-ignore",
   "-string",
   "-int",
   0
};


/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::Prob_to_string                       *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Register type with TCL                               *
*************************************************************************/
const char *RPMPRoblem_Obj::Prob_to_string(rpmProblemType_e p)
{
   for (int i = 0; prob_strings[i].name; ++i)
   {
      if (prob_strings[i].code == p)
         return prob_strings[i].name;
   }
   return "unknown";
}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::MyType::MyType                       *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Register type with TCL                               *
*************************************************************************/
RPMPRoblem_Obj::MyType::MyType(void)
{
   name = "RPMProblem";
   freeIntRepProc   = RPMPRoblem_Obj::FreeInternalRep;
   dupIntRepProc    = RPMPRoblem_Obj::DupInternalRep;
   updateStringProc = RPMPRoblem_Obj::UpdateString;
   setFromAnyProc   = RPMPRoblem_Obj::SetFromAny;
   Tcl_RegisterObjType(this);
}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::MyType::~MyType                      *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   deregister type with TCL                             *
*************************************************************************/
RPMPRoblem_Obj::MyType::~MyType()
{
}

/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::FreeInternalRep                      *
* ARGUMENTS     :   this in disguise                                     *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   free this object (C version)                         *
*************************************************************************/
void RPMPRoblem_Obj::FreeInternalRep(Tcl_Obj *objPtr)
{
   // Paranoia won't destroy ya - is this really a RPMPRoblem_Obj?
   assert(objPtr->typePtr == &mytype);

   RPMPRoblem_Obj *dis = (RPMPRoblem_Obj *)(objPtr->internalRep.otherValuePtr);
   dis->Dec_refcount();
}

/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::DupInternalRep                       *
* ARGUMENTS     :   Source object, destination object                    *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a copy of an RPM problem                      *
*************************************************************************/
void RPMPRoblem_Obj::DupInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
   if (srcPtr->typePtr != &mytype)
   {
      printf("Error: incoming type was %s not %s\n",srcPtr->typePtr->name,mytype.name);
      assert(srcPtr->typePtr == &mytype);
   }
   RPMPRoblem_Obj *from = (RPMPRoblem_Obj *)(srcPtr->internalRep.otherValuePtr);
   dupPtr->internalRep.otherValuePtr = from->Dup();
}

/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::UpdateString                         *
* ARGUMENTS     :   Object to update                                     *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Update the string rep of the object                  *
*************************************************************************/
void RPMPRoblem_Obj::UpdateString(Tcl_Obj *objPtr)
{
   assert(objPtr->typePtr == &mytype);
   RPMPRoblem_Obj *dis = (RPMPRoblem_Obj *)(objPtr->internalRep.otherValuePtr);
   objPtr->bytes = dis->Get_stringrep(objPtr->length);
}

/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::Get_Obj                              *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   Object with refcount of 0                            *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a Tcl_Obj from a problem                      *
*************************************************************************/
Tcl_Obj *RPMPRoblem_Obj::Get_obj(void)
{
   Tcl_Obj *obj = Tcl_NewObj();
   obj->typePtr = &mytype;
   obj->internalRep.otherValuePtr = Dup();
   Tcl_InvalidateStringRep(obj);
   return obj;
}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::SetFromAny                           *
* ARGUMENTS     :   object to set from                                   *
* RETURNS       :   TCL_OK or TCL_ERROR                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a problem from any object                     *
*************************************************************************/
int RPMPRoblem_Obj::SetFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
   // Handle simple case - this is a header object
   if (objPtr->typePtr == &mytype)
      return TCL_OK;
      
    return Cmd_base::Error(interp,"cannot create a %s from a %s\n",objPtr->typePtr->name,mytype.name);
}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::RPMPRoblem_Obj                       *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Set up an empty RPM problem                          *
*************************************************************************/
RPMPRoblem_Obj::RPMPRoblem_Obj(void)
              :ref_count(1)
{
   problem.pkgNEVR       = 0;
   problem.altNEVR       = 0;
   problem.key           = 0;
   problem.type          = RPMPROB_BADARCH;
   problem.ignoreProblem = 1;
   problem.str1          = 0;
   problem.ulong1        = 0;
}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::~RPMPRoblem_Obj                      *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Destroy an RPM problem object                        *
*************************************************************************/
RPMPRoblem_Obj::~RPMPRoblem_Obj()
{
   assert(ref_count == 0);
   // Free strings as needed
   if (problem.pkgNEVR)
      delete [] problem.pkgNEVR;
   if (problem.altNEVR)
      delete [] problem.altNEVR;
   if (problem.str1)
      delete [] problem.str1;
}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::Dup_problem                          *
* ARGUMENTS     :   ref to another problem                               *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a copy of a problem object                    *
*************************************************************************/
void RPMPRoblem_Obj::Dup_problem(const rpmProblem_s &x)
{
   problem.pkgNEVR       = Dup_string(x.pkgNEVR);
   problem.altNEVR       = Dup_string(x.altNEVR);
   problem.str1          = Dup_string(x.str1);
   problem.key           = x.key;
   problem.type          = x.type;
   problem.ignoreProblem = x.ignoreProblem;
   problem.ulong1        = x.ulong1;
}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::RPMPRoblem_Obj                       *
* ARGUMENTS     :   ref to another object                                *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a copy of a header object                     *
*************************************************************************/
RPMPRoblem_Obj::RPMPRoblem_Obj(const RPMPRoblem_Obj &x)
              :ref_count(1)
{
   Dup_problem(x.problem);
}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::Create_from_problem                  *
* ARGUMENTS     :   ref to problem                                       *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a problem Tcl_Obj                             *
*************************************************************************/
Tcl_Obj *RPMPRoblem_Obj::Create_from_problem(const rpmProblem_s &x)
{
   Tcl_Obj *y = Tcl_NewObj();
   assert(y);
   Tcl_IncrRefCount(y);
   Tcl_InvalidateStringRep(y);
   y->typePtr = &mytype;
   y->internalRep.otherValuePtr = new RPMPRoblem_Obj(x);
   return y;
}

/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::RPMPRoblem_Obj                       *
* ARGUMENTS     :   ref to RPM header from library                       *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a header object                               *
*************************************************************************/
RPMPRoblem_Obj::RPMPRoblem_Obj(const rpmProblem_s &x)
              :ref_count(1)
{
   Dup_problem(x);
}

/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::operator =                           *
* ARGUMENTS     :   Object to copy                                       *
* RETURNS       :   ref to this                                          *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Copy one header into another                         *
*************************************************************************/
RPMPRoblem_Obj &RPMPRoblem_Obj::operator =(const RPMPRoblem_Obj &x)
{
   Dup_problem(x.problem);
   return *this;
}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::Get_part                             *
* ARGUMENTS     :   enum of part to get                                  *
* RETURNS       :   part as a Tcl_Obj with a 0 refcount                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Get a part of the problem report                     *
*************************************************************************/
Tcl_Obj *RPMPRoblem_Obj::Get_part(PARTS x)
{
    switch (x)
    {
        case PACKAGE:
        return Tcl_NewStringObj(problem.pkgNEVR?problem.pkgNEVR:"",-1);
        
        case ALT:
        return Tcl_NewStringObj(problem.altNEVR?problem.altNEVR:"",-1);
        
        case KEY:
        return Tcl_NewLongObj((long)problem.key);
        
        case TYPE:
        return Tcl_NewStringObj(Prob_to_string(problem.type),-1);
        
        case IGNORE:
        return Tcl_NewIntObj(problem.ignoreProblem);
        
        case STRING:
        return  Tcl_NewStringObj(problem.str1?problem.str1:"",-1);
        
        case INT:
        return Tcl_NewLongObj(problem.ulong1);
    }
    return 0;
}

/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::Get_parts                            *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   list of parts                                        *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   bust out parts into a problem report                 *
*************************************************************************/
Tcl_Obj *RPMPRoblem_Obj::Get_parts(void)
{
    Tcl_Obj *v[] =
    {
        Part_to_string(TYPE),Get_part(TYPE),
        Part_to_string(PACKAGE),Get_part(PACKAGE),
        Part_to_string(ALT),Get_part(ALT),
        Part_to_string(KEY),Get_part(KEY),
        Part_to_string(IGNORE),Get_part(IGNORE),
        Part_to_string(STRING),Get_part(STRING),
        Part_to_string(INT),Get_part(INT)
    };
    return Tcl_NewListObj((sizeof(v)/sizeof(v[0])),v);

}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::Get_stringrep                        *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   Tcl_Alloc'ed string rep of an object                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Return the string rep of an RPM header               *
*************************************************************************/
char *RPMPRoblem_Obj::Get_stringrep(int &len)
{
   // Get our parts as a TCL list
   Tcl_Obj *name    = Get_parts();
   Tcl_IncrRefCount(name);
   // we must return dynamaically allocated space, so allocate that
   int   size = 0;
   char *from = Tcl_GetStringFromObj(name,&size);
   char *space = Tcl_Alloc(size+1);
   assert(space);
   strncpy(space,from,size);
   space[size] = 0;
   Tcl_DecrRefCount(name);
   len = size;
   return space;
}

/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::Inc_refcount                         *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   new ref count                                        *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Increment the refcount on a header object            *
*************************************************************************/
int RPMPRoblem_Obj::Inc_refcount(void)
{
   return ++ref_count;
}
/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::Dec_refcount                         *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   new refcount                                         *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Dec the refcount on an object.                       *
* NOTE          :   if the refcount goes to zero, the object will be     *
*                   FREED - DO NOT access this object after you have     *
*                   released your refcount!                              *
*************************************************************************/
int RPMPRoblem_Obj::Dec_refcount(void)
{
   int result = --ref_count;
   if (ref_count == 0)
      delete this;
   return result;
}

/*************************************************************************
* FUNCTION      :   RPMPRoblem_Obj::Get_exclusive                        *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   An exclusive reference for the object                *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Insure that no one else has a reference to this      *
*                   to this object - if somebody else has a ref, copy    *
*                   and return the copy.                                 *
*************************************************************************/
RPMPRoblem_Obj *RPMPRoblem_Obj::Get_exclusive(void)
{
   if (ref_count == 1)
      return this;
   RPMPRoblem_Obj *copy = new RPMPRoblem_Obj(*this);
   copy->Inc_refcount();
   return copy;
}
/*************************************************************************
* FUNCTION      :   RPM_for_TCL::NewProblem                              *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create a problem entry                               *
* NOTES         :   format for this is                                   *
*                   RPM problem [part [value]]*                          *
* where <prob> is an existing problem object - if not given, then the    *
* command will create a new problem object.                              *
* [part]  is one of the defined problem tags for a problem               *
* [value] if given will set the defined part                             *
*                                                                        *
*                                                                        *
*************************************************************************/
int RPM_for_TCL::NewProblem(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   RPMPRoblem_Obj *prob = new RPMPRoblem_Obj;
   Tcl_Obj *obj = prob->Get_obj(); // Get an object
   // Now, if the user does something stupid, like
   // RPM problem -ignore
   // he will get back the value of the -ignore field of the object,
   // and the object will go POOF when we dec the refcount.
   // If he does
   // RPM problem -ignore 1
   // Then he will get the ref to the new object and it WON'T poof
   Tcl_IncrRefCount(obj);
   int rv = prob->Problem(interp,objc,objv,prob->Get_obj(),2);
   Tcl_DecrRefCount(obj);
   return rv;
}

/*************************************************************************
* FUNCTION      :   RPMPRoblem_ObjL::Problem                             *
* ARGUMENTS     :   interp, tcl args,                                    *
*                   Problem object                                       *
*                   index in objv of first tag we need                   *
* RETURNS       :   0 if OK, else error                                  *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Manipulate a problem entry                           *
* NOTES         :   format for this is                                   *
*                   RPM [<prob>] [part [value]]*                         *
* where <prob> is an existing problem object - if not given, then the    *
* command will create a new problem object.                              *
* [part]  is one of the defined problem tags for a problem               *
* [value] if given will set the defined part                             *
*                                                                        *
*                                                                        *
*************************************************************************/
int RPMPRoblem_Obj::Problem(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[],
                         Tcl_Obj *prob,int first_tag
                        )
{
   // Now, we have one of 2 possibilities here - they gave us a single tag
   // to query, or a list of (tag,value) pairs
   
   if (objc == (first_tag+1))
   {
      // Single tag - return the value of the tag.
      int which = 0;
      if (Tcl_GetIndexFromObj(interp,objv[first_tag],prob_parts,"tag",0,&which) != TCL_OK)
         return TCL_ERROR;
      Tcl_SetObjResult(interp,Get_part((PARTS)which)); // Return value to TCL
      return TCL_OK;
   }
   Tcl_InvalidateStringRep(prob);

   // OK, so this should be a set of (tag,value) pairs - parse them
   for (int i = first_tag; i < objc; i += 2)
   {
      // Make sure we actually HAVE a value
      if ((i+1)>objc)
         return Cmd_base::Error(interp,"Need a value");
      // what tag is it?
      int which = 0;
      if (Tcl_GetIndexFromObj(interp,objv[i],prob_parts,"tag",0,&which) != TCL_OK)
         return TCL_ERROR;
      switch ((PARTS)which)
      {
         case PACKAGE:
         {
            int len = 0;
            char *x = Tcl_GetStringFromObj(objv[i+1],&len);
            char *p = new char[len+1];
            strncpy(p,x,len);
            p[len] = 0;
            if (problem.pkgNEVR)
               delete [] problem.pkgNEVR;
            problem.pkgNEVR = p;
         }
         break;
         
         case ALT:
         {
            int len = 0;
            char *x = Tcl_GetStringFromObj(objv[i+1],&len);
            char *p = new char[len+1];
            strncpy(p,x,len);
            p[len] = 0;
            if (problem.altNEVR)
               delete [] problem.altNEVR;
            problem.altNEVR = p;
         }
         break;

         case KEY:
         {
            int value = 0;
            if (Tcl_GetIntFromObj(interp,objv[i+1],&value) != TCL_OK)
               return TCL_ERROR;
            problem.key = (fnpyKey)value;
         }
         break;

         case TYPE:
         {
            int which = 0;
            if (Tcl_GetIndexFromObjStruct(interp,objv[i+1],(char **)&prob_strings[0].name,sizeof(prob_strings[0]),
                                 "type",0,&which
                                ) != TCL_OK)
               return TCL_ERROR;
             problem.type = (rpmProblemType)prob_strings[which].code;
         }
         break;

         case IGNORE:
         {
            int value = 0;
            if (Tcl_GetIntFromObj(interp,objv[i+1],&value) != TCL_OK)
               return TCL_ERROR;
            problem.ignoreProblem = value;
         }
         break;

         case STRING:
         {
            int len = 0;
            char *x = Tcl_GetStringFromObj(objv[i+1],&len);
            char *p = new char[len+1];
            strncpy(p,x,len);
            p[len] = 0;
            if (problem.str1)
               delete [] problem.str1;
            problem.str1 = p;
         }
         break;

         case INT:
         {
            int value = 0;
            if (Tcl_GetIntFromObj(interp,objv[i+1],&value) != TCL_OK)
               return TCL_ERROR;
            problem.ulong1 = value;
         }
         break;
      }
   }
   Tcl_SetObjResult(interp,prob); // Return value to TCL
   return TCL_OK;
}
