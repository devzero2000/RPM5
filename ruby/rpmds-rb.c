/**
 * @file ruby/rpmds-rb.c
 * @ingroup rb_c
 *
 * Implementation of the binding to RPM's Dependency Set API.
 */


#include "system.h"
#include "rpm-rb.h"

#include "rpmds-rb.h"

#ifdef	NOTYET
#include <argv.h>
#include <mire.h>
#endif

#define _RPMDS_INTERNAL
#include <rpmtag.h>
#include <rpmtypes.h>
#include <rpmtag.h>
#include <rpmio.h>
#include <rpmds.h>

#include "../debug.h"


VALUE rpmrbDsClass;


/**
 * Struct representing instances of the RPM::Ds class.
 *
 * This struct houses additional maintainance information, like the Ruby
 * object instances that are used to initialize new instances of this class.
 */
struct rpmrbDsInstance_s {
    rpmds ds;   /*< The wrapped rpmds struct */
    VALUE hdr;  /*< Header object used for initialization */
    VALUE tag;  /*< Tag string used for initialization */
};

typedef struct rpmrbDsInstance_s* rpmrbDsInstance;


/*@unchecked@*/
static int _debug = 0;


/**
 * Returns the wrapped rpmrbDsInstance_s:: pointer.
 *
 * @param self  The Ruby class instance
 * @return      The wrapped rpmrbDsInstance_s:: pointer
 */
static rpmrbDsInstance
rpmrbDsiUnwrap(VALUE self)
{
    rpmrbDsInstance dsi = NULL;
    Data_Get_Struct(self, struct rpmrbDsInstance_s, dsi);
    return dsi;
}


rpmds
rpmrbDsUnwrap(VALUE self)
{
    rpmrbDsInstance dsi = rpmrbDsiUnwrap(self);
    return dsi->ds;
}
 

/**
 * Ruby object destructor.
 */
static void
rpmrbDsFree(rpmrbDsInstance dsi)
{
    if (_debug)
        fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, dsi->ds);
    if(NULL != dsi->ds) dsi->ds = rpmdsFree(dsi->ds);
    if(NULL == dsi->ds) free(dsi);
}


/**
 * Marks referenced Ruby objects during garbage collection.
 */
static void
rpmrbDsMark(rpmrbDsInstance dsi)
{
    if(0 != dsi->hdr) rb_gc_mark(dsi->hdr);
    if(0 != dsi->tag) rb_gc_mark(dsi->tag);
}


VALUE
rpmrbDsWrap(rpmds ds)
{
    rpmrbDsInstance dsi = malloc(sizeof(struct rpmrbDsInstance_s));
    dsi = memset(dsi, 0, sizeof(struct rpmrbDsInstance_s));
    dsi->ds = ds;
    return Data_Wrap_Struct(rpmrbDsClass, &rpmrbDsMark, &rpmrbDsFree, dsi);
}


VALUE
rpmrbDsNew(int argc, VALUE *argv, VALUE klass)
{
    VALUE hdr_v, tag_v;
    rpmTag tag = RPMTAG_NAME;
    int flags = 0;
    rpmds ds;
    VALUE rpmrbDs = rpmrbDsWrap(ds);
    rpmrbDsInstance dsi = rpmrbDsiUnwrap(rpmrbDs);

    rb_scan_args(argc, argv, "02", &hdr_v, &tag_v);

    if (!NIL_P(tag_v)) {
        tag = FIX2INT(tag_v);
        dsi->tag = tag_v;
    }

    if (!NIL_P(hdr_v)) {
        Header hdr;
        Data_Get_Struct(hdr_v, struct headerToken_s, hdr);
        ds = rpmdsNew(hdr, tag, flags);
        dsi->hdr = hdr_v;
    } else
        (void) rpmdsRpmlib(&ds, NULL);

    if (_debug)
        fprintf(stderr, "==> %s(%p[%d], 0x%lx) mi %p\n", 
            __FUNCTION__, argv, argc, klass, ds);

    return rpmrbDs;
}

   
static VALUE
rpmrbDsDebugGet(VALUE self)
{
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx)\n", __FUNCTION__, self);
    return INT2FIX(_debug);
}


static VALUE
rpmrbDsDebugSet(VALUE self, VALUE v)
{
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx, 0x%lx)\n", __FUNCTION__, self, v);
    return INT2FIX(_debug = FIX2INT(v));
}



/**
 * Returns the number of elements in the dependency set list.
 *
 * call-seq:
 *  RPM::Ds#count -> Fixnum
 *
 * @return  The current dependency set element count
 * @see     rpmdsCount, rpmds_s::count
 */
static VALUE
rpmrbDsCountGet(VALUE self)
{
    rpmds ds = rpmrbDsUnwrap(self);
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    return INT2FIX(rpmdsCount(ds));
}



/**
 * Get the tag name of the dependecy set.
 *
 * call-seq:
 *  RPM::Ds#type -> String
 *
 * @return  The tag type
 * @see     rpmdsType(), rpmds_s::Type
 */
static VALUE
rpmrbDsTypeGet(VALUE self)
{
    rpmds ds = rpmrbDsUnwrap(self);
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    return rb_str_new2(rpmdsType(ds));
}


/**
 * Returns the current dependency set index.
 *
 * call-seq:
 *  RPM::Ds#ix -> Fixnum
 *
 * @return  Current dependency set index
 * @see     rpmdsIx()
 */
static VALUE
rpmrbDsIxGet(VALUE self)
{
    rpmds ds = rpmrbDsUnwrap(self);
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    return INT2FIX(rpmdsIx(ds));
}


/**
 * Sets a new dependency set index.
 *
 * call-seq:
 *  RPM::Ds#ix = Integer -> Fixnum
 *
 * @param ix    The new index
 * @return      The newly set index
 * @see         rpmdsSetIx()
 */
static VALUE
rpmrbDsIxSet(VALUE self, VALUE v)
{
    rpmds ds = rpmrbDsUnwrap(self);
    int ix = FIX2INT(v);

    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);

    if (ix != rpmdsIx(ds)) {
    	(void) rpmdsSetIx(ds, ix-1);
	    /* XXX flush and recreate N and DNEVR with a rpmdsNext() step */
        (void) rpmdsNext(ds);
    }

    return INT2FIX(rpmdsIx(ds));
}


/**
 * Returns the current build time tie braker.
 *
 * call-seq:
 *  RPM::Ds#bt -> Fixnum
 *
 * @return  The current BT
 * @see     rpmdsBT, rpmds_s::BT
 */
static VALUE
rpmrbDsBTGet(VALUE self)
{
    rpmds ds = rpmrbDsUnwrap(self);
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    return INT2FIX(rpmdsBT(ds));
}


/**
 * Sets the new build time tie braker for the dependency set.
 *
 * call-seq:
 *  RPM::Ds#bt = Integer -> Fixnum
 *
 * @param bt    The new BT
 * @return      The newly set BT
 * @see         rpmdsSetBT(), rpmds_s::BT
 */
static VALUE
rpmrbDsBTSet(VALUE self, VALUE bt_v)
{
    rpmds ds = rpmrbDsUnwrap(self);
    int BT = FIX2INT(bt_v);

    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    
    (void) rpmdsSetBT(ds, BT);
    return INT2FIX(rpmdsBT(ds));
}


/**
 * Get the file colors of the dependency set.
 *
 * call-seq:
 *  RPM::Ds#color -> Fixnum
 *
 * @return  The file colors
 * @see     rpmdsColor(), rpmds_s::Color
 */
static VALUE
rpmrbDsColorGet(VALUE self)
{
    rpmds ds = rpmrbDsUnwrap(self);
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    return INT2FIX(rpmdsColor(ds));
}


/**
 * Sets the color flag of the dependency set.
 *
 * call-seq:
 *  RPM::Ds#color= -> Fixnum
 *
 * @param color The new color flag
 * @return      The result of the operation, i.e. the new color flag
 * @see         rpmdsSetColor, rpmds_s::color
 */
static VALUE
rpmrbDsColorSet(VALUE self, VALUE color_v)
{
    rpmds ds = rpmrbDsUnwrap(self);
    int color = FIX2INT(color_v);

    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);

    (void) rpmdsSetColor(ds, color);
    return INT2FIX(rpmdsColor(ds));
}


/**
 * Returns the current setting of the nopromote flag.
 *
 * call-seq:
 *  RPM::Ds#nopromote -> Fixnum
 *
 * @return  The nopromote flag
 * @see     rpmdsNoPromote(), rpmds_s::nopromote
 */
static VALUE
rpmrbDsNopromoteGet(VALUE self)
{
    rpmds ds = rpmrbDsUnwrap(self);
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    return INT2FIX(rpmdsNoPromote(ds));
}


/**
 * Sets the nopromote flag of the dependency set.
 *
 * call-seq:
 *  RPM::Ds#nopromote = Integer -> Fixnum
 *
 * @param nopromote The new noPromote flag
 * @return          The new flag
 * @see             rpmdsSetNoPromote(), rpmds_s::noPromote
 */
static VALUE
rpmrbDsNopromoteSet(VALUE self, VALUE nopromote_v)
{
    rpmds ds = rpmrbDsUnwrap(self);
    int nopromote = FIX2INT(nopromote_v);

    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);

    (void) rpmdsSetNoPromote(ds, nopromote);
    return self;
}


/**
 * Returns the Name associated with the dependency set.
 *
 * call-seq:
 *  RPM::Ds#name -> String
 *
 * @return  A string with the associated Name
 * @see     rpmdsN(), rpmds_s::Name
 */
static VALUE
rpmrbDsNGet(VALUE self)
{
    rpmds ds = rpmrbDsUnwrap(self);
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    return rb_str_new2(rpmdsN(ds));
}


/**
 * Returns the formatted Epoch:Version-Release string.
 *
 * @return  Formatted EVR string
 * @see     rpmdsEVR(), rpmds_s::EVR
 */
static VALUE
rpmrbDsEVRGet(VALUE self)
{
    rpmds ds = rpmrbDsUnwrap(self);
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    return rb_str_new2(rpmdsEVR(ds));
}


/**
 * Returns the flags of the dependency set instance.
 *
 * call-seq:
 *  RPM::Ds#flags -> Fixnum
 *
 * @return  A bitmask indicating the flags set, 0 on invalid
 * @see     rpmdsFlags(), evrFlags_e::
 */
static VALUE
rpmrbDsFlagsGet(VALUE self)
{
    rpmds ds = rpmrbDsUnwrap(self);
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    return INT2FIX(rpmdsFlags(ds));
}


/**
 * Returns the dependecy string associated with the dependency set.
 * 
 * call-seq:
 *  RPM::Ds#DNEVR -> String
 *
 * @return  The dependency string
 */
static VALUE
rpmrbDsDNEVRGet(VALUE self)
{
    rpmds ds = rpmrbDsUnwrap(self);
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ds);
    return rb_str_new2(rpmdsDNEVR(ds));
}


void
Init_rpmds(void)
{
    rpmrbDsClass = rb_define_class_under(rpmModule, "Ds", rb_cObject);

    if (_debug)
        fprintf(stderr, "==> %s() rpmrbDsClass 0x%lx\n", 
            __FUNCTION__, rpmrbDsClass);

#ifdef  NOTYET
    rb_include_module(rpmrbDsClass, rb_mEnumerable);
#endif

    /* Properties */

    rb_define_method(rpmrbDsClass, "debug", &rpmrbDsDebugGet, 0);
    rb_define_method(rpmrbDsClass, "debug=", &rpmrbDsDebugSet, 1);
    rb_define_method(rpmrbDsClass, "length", &rpmrbDsCountGet, 0);
    rb_define_method(rpmrbDsClass, "ix", &rpmrbDsIxGet, 0);
    rb_define_method(rpmrbDsClass, "ix=", &rpmrbDsIxSet, 1);
    rb_define_method(rpmrbDsClass, "type", &rpmrbDsTypeGet, 0);
    rb_define_method(rpmrbDsClass, "buildtime", &rpmrbDsBTGet, 0);
    rb_define_method(rpmrbDsClass, "buildtime=", &rpmrbDsBTSet, 1);
    rb_define_method(rpmrbDsClass, "color", &rpmrbDsColorGet, 0);
    rb_define_method(rpmrbDsClass, "color=", &rpmrbDsColorSet, 1);
    rb_define_method(rpmrbDsClass, "nopromote", &rpmrbDsNopromoteGet, 0);
    rb_define_method(rpmrbDsClass, "nopromote=", &rpmrbDsNopromoteSet, 1);
    rb_define_method(rpmrbDsClass, "N", &rpmrbDsNGet, 0);
    rb_define_method(rpmrbDsClass, "EVR", &rpmrbDsEVRGet, 0);
    rb_define_method(rpmrbDsClass, "F", &rpmrbDsFlagsGet, 1);
    rb_define_method(rpmrbDsClass, "DNEVR", &rpmrbDsDNEVRGet, 0);

    /* Methods */

    rb_define_singleton_method(rpmrbDsClass, "new", &rpmrbDsNew, -1);
}
