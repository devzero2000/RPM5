#ifndef CMD_BASE_HPP
#define CMD_BASE_HPP
/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : cmd_base.hpp
 AUTHOR      : David Hagood
 DESCRIPTION : RPM bindings for TCL
 
This file implements some of the basics for a verb


*****************************************************************************/
#include <tcl.h>

class Cmd_base
{
public:
   static const Tcl_ObjType * const listtype;
protected:
   struct Cmd_list_base
   {
      const char *cmd; // name of command
      const char *help; // help text for function.
   };
   
   const Cmd_list_base *cmd_ptr;
   int cmd_ptr_size; // sizeof object cmd pointer points to
   
   Tcl_Interp *_interp; // Interp this instance was created for
   Tcl_Command me;

   static int ObjCmd(ClientData dis,Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   static void Interp_Delete(ClientData dis);

   // Utility routines
   Tcl_Obj *Grow_list(Tcl_Obj *list,Tcl_Obj *item); // create a list
   static Tcl_Obj *Grow_list(Tcl_Obj *list,Tcl_Obj *item,Tcl_Interp *interp); // create a list
   static int Cmd_exists(Tcl_Interp *interp,const char *cmd);
   
   // return status manipulation
   static void ReturnF(Tcl_Interp *interp, const char *fmt, va_list args); // format return string
   
   int Error(const char *fmt,...); // set error status and return
   int OK(const char *fmt,...); //Return OK status to TCL
   int OK(Tcl_Obj *x); //Return object to TCL, drop refcount, and return OK
   int OK(int x); //Return object to TCL and return OK
   int Return(int rc,Tcl_Obj *result);
   
public:
   static int Error(Tcl_Interp *interp, const char *fmt,...); // static version

   Cmd_base(Tcl_Interp *interp,const char *name,const Cmd_list_base *cmds,size_t cmd_size);
   virtual ~Cmd_base();
   
   void Delete(void); // Remove from interp
  
   // Common functions
   virtual int Help(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   virtual int Cmd(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
   
   // Implementation hooks
   virtual int Exec(int which,Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]) = 0;
   virtual int Cmd_hook(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[]);
};


#endif
