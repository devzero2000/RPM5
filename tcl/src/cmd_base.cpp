/*****************************************************************************
* Aeroflex, Inc        COPYRIGHT (C)2004      DESIGN ENGINEERING
******************************************************************************
 FILE        : cmd_base.cpp
 AUTHOR      : David Hagood
 DESCRIPTION : Basic TCL command functinos
 
This file implements the basics for TCL verbs

*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include "cmd_base.hpp"
#include "config.h"

// util variables
const Tcl_ObjType * const Cmd_base::listtype = Tcl_GetObjType("list");

/*************************************************************************
* FUNCTION      :   Cmd_base::ReturnF                                    *
* ARGUMENTS     :   interp - interp to return message to                 *
*                   char *fmt - sprintf style format string.             *
*                   any other args you want                              *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   "printf" to the TCL return variable                  *
*************************************************************************/
void Cmd_base::ReturnF(Tcl_Interp *interp, const char *fmt, va_list args)
{
   // This is not intended to be perfect - you REALLY shouldn't be using
   // big honkin' reports for this. So we will use a fixed size buffer
   // and limit ourselves to how much we fill it.
   const size_t max_len = 4096;
   Tcl_Obj *ptr = Tcl_NewStringObj("",0);
   Tcl_SetObjLength(ptr,max_len);
   char *string = Tcl_GetStringFromObj(ptr,0);
   //OK, so we have a TCL string of at least max_len size. Now populate it
   vsnprintf(string,max_len,fmt,args);
   Tcl_SetObjLength(ptr,strlen(string)); // Trim off any unused length
   Tcl_SetObjResult(interp,ptr);
}

/*************************************************************************
* FUNCTION      :   Error                                                *
* ARGUMENTS     :   interp - the interp to set up                        *
*                   char *fmt - sprintf style format string.             *
*                   any other args you want                              *
* RETURNS       :   TCL_ERROR                                            *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Report an error back to TCL                          *
*************************************************************************/
int Cmd_base::Error(Tcl_Interp *interp,const char *fmt,...)
{
   va_list args;
   va_start(args,fmt);
   
   ReturnF(interp,fmt,args);
   
   va_end(args);
   return TCL_ERROR;
}
/*************************************************************************
* FUNCTION      :   Error                                                *
* ARGUMENTS     :   char *fmt - sprintf style format string.             *
*                   any other args you want                              *
* RETURNS       :   TCL_ERROR                                            *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Report an error back to TCL                          *
*************************************************************************/
int Cmd_base::Error(const char *fmt,...)
{
   va_list args;
   va_start(args,fmt);
   
   ReturnF(_interp,fmt,args);
   
   va_end(args);
   return TCL_ERROR;
}

/*************************************************************************
* FUNCTION      :   OK                                                   *
* ARGUMENTS     :   char *fmt - sprintf style format string.             *
*                   any other args you want                              *
* RETURNS       :   TCL_OK                                               *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Report OK status back to TCL                         *
*************************************************************************/
int Cmd_base::OK(const char *fmt,...)
{
   va_list args;
   va_start(args,fmt);
   
   ReturnF(_interp,fmt,args);
   
   va_end(args);
   return TCL_OK;
}

/*************************************************************************
* FUNCTION      :   OK                                                   *
* ARGUMENTS     :   TCL object to return (NULL allowed)                  *
* RETURNS       :   TCL_OK                                               *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Report OK status back to TCL                         *
*************************************************************************/
int Cmd_base::OK(Tcl_Obj *value)
{
   if (value)
   {
     Tcl_SetObjResult(_interp,value); // Return value to TCL
     Tcl_DecrRefCount(value); // Drop our refcount
   }
   return TCL_OK;
}
/*************************************************************************
* FUNCTION      :   OK                                                   *
* ARGUMENTS     :   int to return                                        *
* RETURNS       :   TCL_OK                                               *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Report OK status back to TCL                         *
*************************************************************************/
int Cmd_base::OK(int value)
{
   Tcl_SetObjResult(_interp,Tcl_NewIntObj(value)); // Return value to TCL
   return TCL_OK;
}
/*************************************************************************
* FUNCTION      :   Return                                               *
* ARGUMENTS     :   return code value, possible TCL result               *
* RETURNS       :   return code                                          *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Report status back to TCL, returning value if TCL_OK,*
*                   and in any case decrementing the refcount of value   *
*************************************************************************/
int Cmd_base::Return(int value,Tcl_Obj *result)
{
   if (result)
   {
      if (value == TCL_OK)
         Tcl_SetObjResult(_interp,result); // Return value to TCL
      Tcl_DecrRefCount(result);
   }
   return value;
}
/*************************************************************************
* FUNCTION      :   Cmd_base::Cmd_base                                   *
* ARGUMENTS     :   interp, name, command list                           *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Create the RPM command object                        *
*************************************************************************/
Cmd_base::Cmd_base(Tcl_Interp *interp,const char *name,const Cmd_list_base *cmds,size_t cmd_size)
            :cmd_ptr(cmds),cmd_ptr_size(cmd_size),_interp(interp)
{
   me = Tcl_CreateObjCommand(interp, name, ObjCmd, (ClientData)this,
                        (Tcl_CmdDeleteProc *)Interp_Delete
                       );
}

/*************************************************************************
* FUNCTION      :   Cmd_base::~Cmd_base                                  *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Destroy the command object                           *
*************************************************************************/
Cmd_base::~Cmd_base()
{
   Tcl_DeleteCommandFromToken(_interp,me); // Remove my command
}
/*************************************************************************
* FUNCTION      :   Cmd_base::Delete                                     *
* ARGUMENTS     :   none                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Destroy the command object                           *
*************************************************************************/
void Cmd_base::Delete(void)
{
   Tcl_DeleteCommandFromToken(_interp,me); // Remove my command
}
/*************************************************************************
* FUNCTION      :   Cmd_base::Interp_Delete                              *
* ARGUMENTS     :   this                                                 *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   cleanup on removal from an interp                    *
*************************************************************************/
void Cmd_base::Interp_Delete(ClientData dis)
{
   delete (Cmd_base *)dis;
}

/*************************************************************************
* FUNCTION      :   Cmd_base::Cmd_exists                                 *
* ARGUMENTS     :   interp to check, cmd to check                        *
* RETURNS       :   1 if cmd exists, else 0                              *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   See if a command already exists within an interp     *
*************************************************************************/
int Cmd_base::Cmd_exists(Tcl_Interp *interp,const char *cmd)
{
   Tcl_CmdInfo cmdInfo;
   if (Tcl_GetCommandInfo(interp, cmd, &cmdInfo))
      return 1;
   return 0;
}

/*************************************************************************
* FUNCTION      :   Cmd_base::Grow_list                                  *
* ARGUMENTS     :   list to grow, item to add                            *
* RETURNS       :   newly updated list or 0 if error                     *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Grow a list of items                                 *
*************************************************************************/
Tcl_Obj *Cmd_base::Grow_list(Tcl_Obj *list,Tcl_Obj *item)
{
   return Grow_list(list,item,_interp);
}
Tcl_Obj *Cmd_base::Grow_list(Tcl_Obj *list,Tcl_Obj *item,Tcl_Interp *_interp)
{
   if (!item)
      return list;
      
   if (!list)
   {
      list = Tcl_NewListObj(1,&item);
      Tcl_IncrRefCount(list);
      return list;
   }
   // is the item already a list? If not, make a list of the 2 items
   if (list->typePtr == listtype)
   {
      if (Tcl_ListObjAppendElement(_interp,list,item) != TCL_OK)
      {
         Tcl_DecrRefCount(list);
         return 0;
      }
      return list;
   }
   // OK, morph into a list
   Tcl_Obj *objv[2] = { list,item};
   Tcl_Obj *l = Tcl_NewListObj(2,objv);
   Tcl_IncrRefCount(l);
   Tcl_DecrRefCount(list);
   return l;
}

/*************************************************************************
* FUNCTION      :   Cmd_base::ObjCmd                                     *
* ARGUMENTS     :   interp                                               *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   C compatible entry point for a command               *
*************************************************************************/
int Cmd_base::ObjCmd(ClientData dis,Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   return ((Cmd_base *)dis)->Cmd(interp,objc,objv);
}

/*************************************************************************
* FUNCTION      :   Cmd_base::Cmd_hook                                   *
* ARGUMENTS     :   interp, args from TCL                                *
* RETURNS       :   TCL_RETURN if we did not handle the command          *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Command hook before standard processing              *
*************************************************************************/
int Cmd_base::Cmd_hook(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   return TCL_RETURN;
}

/*************************************************************************
* FUNCTION      :   Cmd_base::Cmd                                        *
* ARGUMENTS     :   interp, args from TCL                                *
* RETURNS       :   none                                                 *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Carry out the RPM command                            *
*************************************************************************/
int Cmd_base::Cmd(Tcl_Interp *interp,int objc, Tcl_Obj *const objv[])
{
   int which = 0;
   int rv = TCL_OK;
   
   if (objc < 2)
   {
      Help(interp,objc,objv);
      return TCL_ERROR;
   }
   // Let the derived class have a crack at it
   rv = Cmd_hook(interp,objc,objv);
   if (rv != TCL_RETURN)
      return rv;
      
   if (Tcl_GetIndexFromObjStruct(interp,objv[1],(char **)&cmd_ptr[0].cmd,cmd_ptr_size,
                                 "subcommand",0,&which
                                ) != TCL_OK)
      return TCL_ERROR;

   return Exec(which,interp,objc,objv);
}

/*************************************************************************
* FUNCTION      :   Cmd_base::Help                                       *
* ARGUMENTS     :   interp, tcl args                                     *
* RETURNS       :   TCL_OK                                               *
* EXCEPTIONS    :   none                                                 *
* PURPOSE       :   Return help text                                     *
*************************************************************************/
int Cmd_base::Help(Tcl_Interp *interp,int , Tcl_Obj *const [])
{
   // since we don't know how big the cmd_ptr is, we have to trick it
   union
   {
      const char *cp;
      const Cmd_list_base *bp;
   } ptr;
   ptr.bp = cmd_ptr;
   
   for (int i = 0; ptr.bp->cmd != 0; ++i)
   {
      Tcl_AppendResult(interp,ptr.bp->cmd," - ",ptr.bp->help,"\n",0);
      ptr.cp += cmd_ptr_size;
   }
   return TCL_OK;
}
