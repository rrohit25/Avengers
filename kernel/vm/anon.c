#include "globals.h"
#include "errno.h"

#include "util/string.h"
#include "util/debug.h"

#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "mm/slab.h"
#include "mm/tlb.h"

int anon_count = 0; /* for debugging/verification purposes */

static slab_allocator_t *anon_allocator;

static void anon_ref(mmobj_t *o);
static void anon_put(mmobj_t *o);
static int anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int anon_fillpage(mmobj_t *o, pframe_t *pf);
static int anon_dirtypage(mmobj_t *o, pframe_t *pf);
static int anon_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t anon_mmobj_ops = {
       .ref = anon_ref,
       .put = anon_put,
       .lookuppage = anon_lookuppage,
       .fillpage = anon_fillpage,
       .dirtypage = anon_dirtypage,
       .cleanpage = anon_cleanpage
};

/*
* This function is called at boot time to initialize the
* anonymous page sub system. Currently it only initializes the
* anon_allocator object.
*/
void
anon_init()
{
       anon_allocator = slab_allocator_create("anonobj", sizeof(mmobj_t));
       KASSERT(NULL != anon_allocator);
       dbg(DBG_PRINT, "(GRADING3A 4.a)anon object is successfully created\n "); 
/* NOT_YET_IMPLEMENTED("VM: anon_init");*/
}

/*
* You'll want to use the anon_allocator to allocate the mmobj to
* return, then then initialize it. Take a look in mm/mmobj.h for
* macros which can be of use here. Make sure your initial
* reference count is correct.
*/
mmobj_t *
anon_create()
{
       /*NOT_YET_IMPLEMENTED("VM: anon_create");*/
       mmobj_t *anoncreate = (mmobj_t*)slab_obj_alloc(anon_allocator);
       if(anoncreate)
       {
	 memset(anoncreate,0,sizeof(anoncreate));
	 list_init(&anoncreate->mmo_respages);
         list_init(&anoncreate->mmo_un.mmo_vmas);

         mmobj_init(anoncreate,&anon_mmobj_ops);        
         anoncreate->mmo_refcount++;
       }      
       return anoncreate;
}

/* Implementation of mmobj entry points: */

/*
* Increment the reference count on the object.
*/
static void
anon_ref(mmobj_t *o)
{
	 KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));        
       dbg(DBG_PRINT, "(GRADING3A 4.b)Reference count of the anon object is greater than 0\n ");
	o->mmo_refcount++;
}

/*
* Decrement the reference count on the object. If, however, the
* reference count on the object reaches the number of resident
* pages of the object, we can conclude that the object is no
* longer in use and, since it is an anonymous object, it will
* never be used again. You should unpin and uncache all of the
* object's pages and then free the object itself.
*/
static void
anon_put(mmobj_t *o)
{
    KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));        
dbg(DBG_PRINT, "(GRADING3A 4.c)Reference count of the anon object is greater than zero\n ");
o->mmo_refcount--;
       if(o->mmo_nrespages == o->mmo_refcount &&
	  o->mmo_nrespages != 0)
             {
                        pframe_t *i = NULL;
                        list_iterate_begin(&(o->mmo_respages), i, pframe_t, pf_olink)
                           {
                               while(pframe_is_pinned(i))
                                       pframe_unpin(i);
                               if (pframe_is_busy(i))
                                      sched_sleep_on(&i->pf_waitq);
                               else if (pframe_is_dirty(i))
                                       pframe_clean(i);
                               else
                                       pframe_free(i);
                               
                            }list_iterate_end();
               }
       
        else if(0 == o->mmo_refcount && 0 == o->mmo_nrespages )
       {
                o = NULL;
                slab_obj_free(anon_allocator, o);
       }
       /* NOT_YET_IMPLEMENTED("VM: shadow_put");*/

}

/* Get the corresponding page from the mmobj. No special handling is
* required. */
static int
anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
       pframe_t *pflookup = pframe_get_resident(o,pagenum);
       if(pflookup)
       {
               while(!pframe_is_busy(pflookup))
               {
                       *pf = pflookup;
                        return 0;
               }
       }
       return -1; 
       
       /*NOT_YET_IMPLEMENTED("VM: anon_lookuppage");*/
}

static int
anon_fillpage(mmobj_t *o,pframe_t *pf)
{
 /*NOT_YET_IMPLEMENTED("VM: anon_fillpage");*/
       
        KASSERT(pframe_is_busy(pf));        
       dbg(DBG_PRINT, "(GRADING3A 4.d)PF_BUSY flag set for the page frame\n ");
	 KASSERT(!pframe_is_pinned(pf));    
	dbg(DBG_PRINT, "(GRADING3A 4.d)Page frame is NOT pinned\n ");    
               memset(pf->pf_addr,0,PAGE_SIZE);
	return 0;
               
}

static int
anon_dirtypage(mmobj_t *o, pframe_t *pf)
{
       return 0;
}

static int
anon_cleanpage(mmobj_t *o, pframe_t *pf)
{
        /*NOT_YET_IMPLEMENTED("VM: anon_cleanpage");*/
        return 0;
}
