/*
 *    Copyright [2013] [Ramon Fried] <ramon.fried at gmail dot com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "convert.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

#define LOG_FUNC_ENTRANCE() printf("%lu: In %s\n", time(NULL), __PRETTY_FUNCTION__);

#define WRITE_LOCK(list, err_code) \
    if ( TRUE == (( _llist * )list)->ismt ) \
    { \
        int rc = pthread_rwlock_wrlock(&( ( _llist * ) list )->llist_lock); \
        if (rc != 0)\
        {\
            return err_code;\
        }\
    }

#define READ_LOCK(list, err_code) \
    if ( TRUE == (( _llist * )list)->ismt ) \
    { \
        int rc = pthread_rwlock_rdlock(&( ( _llist * ) list )->llist_lock); \
        if (rc != 0)\
        {\
            return err_code;\
        }\
    }

#define UNLOCK(list, err_code) \
    if ( TRUE == (( _llist * )list)->ismt ) \
    { \
        int rc = pthread_rwlock_unlock(&( ( _llist * ) list )->llist_lock); \
        if (rc != 0)\
        {\
            return err_code;\
        }\
    }

typedef struct __list_node
{
    llist_node node;
    struct __list_node *next;
} _list_node;

typedef struct
{
    unsigned int count;
    comperator comp_func;
    equal equal_func;
    _list_node *head;
    _list_node *tail;

    //multi-threading support
    unsigned char ismt;
    pthread_rwlockattr_t llist_lock_attr;
    pthread_rwlock_t llist_lock;

} _llist;

/* Helper functions - not to be exported */
static _list_node *listsort ( _list_node *list, _list_node ** updated_tail, comperator cmp, int flags );

llist llist_create ( comperator compare_func, equal equal_func, unsigned flags)
{
    _llist *new_list;
    int rc = 0;
    new_list = malloc ( sizeof ( _llist ) );

    if ( new_list == NULL )
    {
        return NULL;
    }

// These can be NULL, I don't care...
    new_list->equal_func = equal_func;
    new_list->comp_func = compare_func;

// Reset the list
    new_list->count = 0;
    new_list->head = NULL;
    new_list->tail = NULL;

    new_list->ismt = FALSE;
    if ( flags & MT_SUPPORT_TRUE)
    {
        new_list->ismt = TRUE;
        rc = pthread_rwlockattr_setpshared( &new_list->llist_lock_attr,  PTHREAD_PROCESS_PRIVATE );
        if ( 0 != rc)
        {
            free(new_list);
            return NULL;
        }
        rc = pthread_rwlock_init( &new_list->llist_lock, &new_list->llist_lock_attr );
        if ( 0 != rc)
        {
            pthread_rwlockattr_destroy(&new_list->llist_lock_attr);
            free(new_list);
            return NULL;
        }
    }

    return new_list;
}

void llist_destroy ( llist list, bool destroy_nodes, node_func destructor )
{
    _list_node *iterator;
    _list_node *next;

    if ( list == NULL )
    {
        return;
    }
    // Delete the data contained in the nodes
    iterator = ( ( _llist * ) list )->head;

    while ( iterator != NULL )
    {

        if ( destroy_nodes )
        {

            if ( destructor )
            {
                destructor ( iterator->node );
            }
            else
            {
                free ( iterator->node );
            }
        }

        next = iterator->next;

        free ( iterator ); // Delete's the container

        iterator = next;
    }



    if ( TRUE == (( _llist * )list)->ismt )
    {
        //release any thread related resource, just try to destroy no use checking return code
        pthread_rwlockattr_destroy(&( ( _llist * ) list )->llist_lock_attr);
        pthread_rwlock_destroy( &( ( _llist * ) list )->llist_lock);
    }
    //release the list
    free ( list );

    return;

}

int llist_size ( llist list )
{
    unsigned int retval = 0;
    if ( list == NULL )
    {
        return 0;
    }
    READ_LOCK( list, LLIST_MULTITHREAD_ISSUE )

    {
        //read only critical section
        retval = ( ( _llist * ) list )->count;
    }

    UNLOCK( list, LLIST_MULTITHREAD_ISSUE )

    return retval;
}

int llist_add_node ( llist list, llist_node node, int flags )
{
    _list_node *node_wrapper = NULL;

    if ( list == NULL )
    {
        return LLIST_NULL_ARGUMENT;
    }

    WRITE_LOCK( list, LLIST_MULTITHREAD_ISSUE )

    {   //write critical section
        node_wrapper = malloc ( sizeof ( _list_node ) );

        if ( node_wrapper == NULL )
        {
            UNLOCK( list, LLIST_MULTITHREAD_ISSUE )
            return LLIST_ERROR;
        }

        node_wrapper->node = node;

        ( ( _llist * ) list )->count++;

        if ( ( ( _llist * ) list )->head == NULL ) // Adding the first node, update head and tail to point to that node
        {
            node_wrapper->next = NULL;
            ( ( _llist * ) list )->head = ( ( _llist * ) list )->tail = node_wrapper;
        }
        else if ( flags & ADD_NODE_FRONT )
        {
            node_wrapper->next = ( ( _llist * ) list )->head;
            ( ( _llist * ) list )->head = node_wrapper;
        }
        else   // add node in the rear
        {
            node_wrapper->next = NULL;
            ( ( _llist * ) list )->tail->next = node_wrapper;
            ( ( _llist * ) list )->tail = node_wrapper;
        }
    }

    UNLOCK( list, LLIST_MULTITHREAD_ISSUE )

    return LLIST_SUCCESS;
}

int llist_delete_node ( llist list, llist_node node,
		bool destroy_node, node_func destructor )
{
    _list_node *iterator;
    _list_node *temp;
    equal actual_equal;

    if ( ( list == NULL ) || ( node == NULL ) )
    {
        return LLIST_NULL_ARGUMENT;
    }

    actual_equal = ( ( _llist * ) list )->equal_func;

	if ( actual_equal == NULL )
    {
        return LLIST_EQUAL_MISSING;
    }

    WRITE_LOCK( list, LLIST_MULTITHREAD_ISSUE )

    {

        iterator = ( ( _llist * ) list )->head;

        if ( NULL == iterator)
        {
            UNLOCK( list, LLIST_MULTITHREAD_ISSUE );
            return LLIST_NODE_NOT_FOUND;
        }


        // is it the first node ?
        if ( actual_equal ( iterator->node, node ) )
        {
            if ( destroy_node )
            {
                if ( destructor )
                {
                    destructor ( iterator->node);
                }
                else
                {
                    free ( iterator->node );
                }

            }

            ( ( _llist * ) list )->head = ( ( _llist * ) list )->head->next;
            free ( iterator );
            ( ( _llist * ) list )->count--;

            if ( ( ( _llist * ) list )->count == 0 )
            {
                /*
                 *  if we deleted the last node, we need to reset the tail also
                 *  There's no need to check it somewhere else, because the last node must be the head (and tail)
                 */
                ( ( _llist * ) list )->tail = NULL;
            }
            //assert ( ( ( _llist * ) list )->count >= 0 );
            UNLOCK( list, LLIST_MULTITHREAD_ISSUE );
            return LLIST_SUCCESS;
        }
        else
        {
            while ( iterator->next != NULL )
            {
                if ( actual_equal ( iterator->next->node, node ) )
                {
                    // found it
                    temp = iterator->next;
                    iterator->next = temp->next;
                    free ( temp );

                    ( ( _llist * ) list )->count--;
                    //assert ( ( ( _llist * ) list )->count >= 0 );

                    UNLOCK( list, LLIST_MULTITHREAD_ISSUE );
                    return LLIST_SUCCESS;
                }

                iterator = iterator->next;
            }
        }

        if ( iterator->next == NULL )
        {
            UNLOCK( list,LLIST_MULTITHREAD_ISSUE );
            return LLIST_NODE_NOT_FOUND;
        }

    }

    //assert ( 1 == 2 );
    // this assert always failed. we assume that the function never gets here...
    UNLOCK( list, LLIST_MULTITHREAD_ISSUE );
    return LLIST_ERROR;
}

int llist_for_each ( llist list, node_func func )
{
    _list_node *iterator;

    if ( ( list == NULL ) || ( func == NULL ) )
    {
        return LLIST_NULL_ARGUMENT;
    }

    iterator = ( ( _llist * ) list )->head;

    while ( iterator != NULL )
    {
        func ( iterator->node );
        iterator = iterator->next;
    }

    return LLIST_SUCCESS;
}

int llist_for_each_arg ( llist list, node_func_arg func, void * arg )
{
    _list_node *iterator;

    if ( ( list == NULL ) || ( func == NULL ) )
    {
        return LLIST_NULL_ARGUMENT;
    }

    READ_LOCK( list, LLIST_MULTITHREAD_ISSUE )

    {

        iterator = ( ( _llist * ) list )->head;

        while ( iterator != NULL )
        {
            func ( iterator->node , arg);
            iterator = iterator->next;
        }
    }

    UNLOCK( list, LLIST_MULTITHREAD_ISSUE )

    return LLIST_SUCCESS;
}

int llist_insert_node ( llist list, llist_node new_node, llist_node pos_node,
                        int flags )
{
    _list_node *iterator;
    _list_node *node_wrapper = NULL;

    if ( ( list == NULL ) || ( new_node == NULL ) || ( pos_node == NULL ) )
    {
        return LLIST_NULL_ARGUMENT;
    }

    WRITE_LOCK( list, LLIST_MULTITHREAD_ISSUE )

    {
        node_wrapper = malloc ( sizeof ( _list_node ) );

        if ( node_wrapper == NULL )
        {
            UNLOCK( list, LLIST_MULTITHREAD_ISSUE )
            return LLIST_MALLOC_ERROR;
        }

        node_wrapper->node = new_node;

        ( ( _llist * ) list )->count++;

        iterator = ( ( _llist * ) list )->head;

        if ( iterator->node == pos_node )
        {
            // it's the first node

            if ( flags & ADD_NODE_BEFORE )
            {
                node_wrapper->next = iterator;
                ( ( _llist * ) list )->head = node_wrapper;
            }
            else
            {
                node_wrapper->next = iterator->next;
                iterator->next = node_wrapper;
            }
            UNLOCK( list, LLIST_MULTITHREAD_ISSUE )
            return LLIST_SUCCESS;
        }

        while ( iterator->next != NULL )
        {
            if ( iterator->next->node == pos_node )
            {
                if ( flags & ADD_NODE_BEFORE )
                {
                    node_wrapper->next = iterator->next;
                    iterator->next = node_wrapper;
                }
                else
                {
                    iterator = iterator->next;
                    // now we stand on the pos node
                    node_wrapper->next = iterator->next;
                    iterator->next = node_wrapper;
                }
                UNLOCK( list, LLIST_MULTITHREAD_ISSUE )
                return LLIST_SUCCESS;
            }

            iterator = iterator->next;
        }

    }
    UNLOCK( list, LLIST_MULTITHREAD_ISSUE )

    assert ( 1 == 2 );
    // this assert will always fail. we assume that the function never gets here...
    return LLIST_ERROR;

}

int llist_find_node ( llist list, void *data, llist_node *found)
{
    _list_node *iterator;
    equal actual_equal;
    if ( list == NULL )
    {
        return LLIST_NULL_ARGUMENT;
    }

    actual_equal = ( ( _llist * ) list )->equal_func;

    if ( actual_equal == NULL )
    {
        return LLIST_EQUAL_MISSING;
    }

    READ_LOCK( list, LLIST_MULTITHREAD_ISSUE )

    {
        iterator = ( ( _llist * ) list )->head;

        while ( iterator != NULL )
        {
            if ( actual_equal ( iterator->node, data ) )
            {

                *found = iterator->node;
                UNLOCK( list, LLIST_MULTITHREAD_ISSUE )
                return LLIST_SUCCESS;
            }

            iterator = iterator->next;
        }
    }

    UNLOCK( list, LLIST_MULTITHREAD_ISSUE )

    // Didn't find the node
    return LLIST_NODE_NOT_FOUND;

}

llist_node llist_get_head ( llist list )
{
    READ_LOCK( list, NULL )

    {
        if ( list != NULL )
        {
            if ( ( ( _llist * ) list )->head ) // there's at least one node
            {
                UNLOCK( list, NULL )
                return ( ( _llist * ) list )->head->node;
            }
        }
    }

    UNLOCK( list, NULL )

    return NULL;
}

llist_node llist_get_tail ( llist list )
{
    READ_LOCK( list, NULL )

    {
        if ( list != NULL )
        {
            if ( ( ( _llist * ) list )->tail ) // there's at least one node
            {
                UNLOCK(list, NULL)
                return ( ( _llist * ) list )->tail->node;
            }
        }
     }

    UNLOCK(list, NULL)

    return NULL;
}



int llist_push ( llist list, llist_node node )
{
    return llist_add_node ( list, node, ADD_NODE_FRONT );
}

llist_node llist_peek ( llist list )
{
    return llist_get_head ( list );
}



llist_node llist_pop ( llist list )
{
    llist_node tempnode = NULL;
    _list_node *tempwrapper;

    WRITE_LOCK( list, NULL )

    {
        if ( ( ( _llist * ) list )->count ) // There exists at least one node
        {
            tempwrapper = ( ( _llist * ) list )->head;
            tempnode = tempwrapper->node;
            ( ( _llist * ) list )->head = ( ( _llist * ) list )->head->next;
            ( ( _llist * ) list )->count--;
            free ( tempwrapper );

            if ( ( ( _llist * ) list )->count == 0 ) // We've deleted the last node
            {
                ( ( _llist * ) list )->tail = NULL;
            }
        }
    }

    UNLOCK(list, NULL)

    return tempnode;
}





int llist_concat ( llist first, llist second )
{
    _list_node *end_node;

    if ( ( first == NULL ) || ( second == NULL ) )
    {
        return LLIST_NULL_ARGUMENT;
    }

    WRITE_LOCK( first, LLIST_MULTITHREAD_ISSUE )
    WRITE_LOCK( second, LLIST_MULTITHREAD_ISSUE )

    {

        end_node = ( ( _llist * ) first )->tail;

        ( ( _llist * ) first )->count += ( ( _llist * ) second )->count;

        if ( end_node != NULL ) // if the first list is not empty
        {
            end_node->next = ( ( _llist * ) second )->head;
        }
        else     // It's empty
        {
            ( ( _llist * ) first )->head = ( ( _llist * ) first )->tail =
                    ( ( _llist * ) second )->head;
        }

        // Delete the nodes from the second list. (not really deletes them, only loses their reference.
        ( ( _llist * ) second )->count = 0;
        ( ( _llist * ) second )->head = ( ( _llist * ) second )->tail = NULL;
    }

    UNLOCK( first, LLIST_MULTITHREAD_ISSUE )
    UNLOCK( second, LLIST_MULTITHREAD_ISSUE )

    return LLIST_SUCCESS;
}

int llist_reverse ( llist list )
{
    if ( list == NULL )
    {
        return LLIST_NULL_ARGUMENT;
    }


    WRITE_LOCK( list, LLIST_MULTITHREAD_ISSUE )

    {

        _list_node *iterator = ( ( _llist * ) list )->head;
        _list_node *nextnode = NULL;
        _list_node *temp = NULL;

        /*
         * Swap our Head & Tail pointers
         */
        ( ( _llist * ) list )->head = ( ( _llist * ) list )->tail;
        ( ( _llist * ) list )->tail = iterator;

        /*
         * Swap the internals
         */
        while ( iterator )
        {
            nextnode = iterator->next;
            iterator->next = temp;
            temp = iterator;
            iterator = nextnode;
        }

    }

    UNLOCK( list, LLIST_MULTITHREAD_ISSUE )

    return LLIST_SUCCESS;
}

int llist_sort ( llist list, int flags )
{

    comperator cmp;
    if ( list == NULL )
    {
        return LLIST_NULL_ARGUMENT;
    }

 	_llist *thelist = ( _llist * ) list;

    cmp =  thelist->comp_func;

 	if ( cmp == NULL )
    {
        return LLIST_COMPERATOR_MISSING;
    }
    WRITE_LOCK( list, LLIST_MULTITHREAD_ISSUE )
    thelist->head = listsort ( thelist->head, &thelist->tail, cmp, flags);
    UNLOCK( list, LLIST_MULTITHREAD_ISSUE )
    /*
     * TODO: update list tail.
     */
    return LLIST_SUCCESS;
}

static _list_node *listsort ( _list_node *list, _list_node ** updated_tail, comperator cmp , int flags)
{
    _list_node *p, *q, *e, *tail;
    int insize, nmerges, psize, qsize, i;
    int direction = ( flags & SORT_LIST_ASCENDING ) ? 1 : -1;
    insize = 1;

    while ( 1 )
    {
        p = list;
        list = NULL;
        tail = NULL;

        nmerges = 0; /* count number of merges we do in this pass */

        while ( p )
        {
            nmerges++; /* there exists a merge to be done */
            /* step `insize' places along from p */
            q = p;
            psize = 0;
            for ( i = 0; i < insize; i++ )
            {
                psize++;
                q = q->next;
                if ( !q )
                {
                    break;
                }
            }

            /* if q hasn't fallen off end, we have two lists to merge */
            qsize = insize;

            /* now we have two lists; merge them */
            while ( psize > 0 || ( qsize > 0 && q ) )
            {

                /* decide whether next element of merge comes from p or q */
                if ( psize == 0 )
                {
                    /* p is empty; e must come from q. */
                    e = q;
                    q = q->next;
                    qsize--;
                }
                else
                    if ( qsize == 0 || !q )
                    {
                        /* q is empty; e must come from p. */
                        e = p;
                        p = p->next;
                        psize--;
                    }
                    else
                        if ( ( direction * cmp ( p->node, q->node ) ) <= 0 )
                        {
                            /* First element of p is lower (or same);
                             * e must come from p. */
                            e = p;
                            p = p->next;
                            psize--;
                        }
                        else
                        {
                            /* First element of q is lower; e must come from q. */
                            e = q;
                            q = q->next;
                            qsize--;
                        }

                /* add the next element to the merged list */
                if ( tail )
                {
                    tail->next = e;
                }
                else
                {
                    list = e;
                }

                tail = e;
            }

            /* now p has stepped `insize' places along, and q has too */
            p = q;
        }

        tail->next = NULL;

        /* If we have done only one merge, we're finished. */
        if ( nmerges <= 1 ) /* allow for nmerges==0, the empty list case */
        {
            break;
        }
        /* Otherwise repeat, merging lists twice the size */
        insize *= 2;
    }

    *updated_tail = tail;
    return list;
}

static int llist_get_min_max(llist list, llist_node * output, bool max)
{
    comperator cmp;

	if ( list == NULL )
	{
		return LLIST_NULL_ARGUMENT;
	}

    cmp =  ( ( _llist * ) list )->comp_func;

 	if ( cmp == NULL )
    {
        return LLIST_COMPERATOR_MISSING;
    }

	_list_node *iterator = ( ( _llist * ) list )->head;
	*output = iterator->node;
	iterator = iterator->next;
	while (iterator)
	{
		if (max) // Find maximum
		{
			if ( cmp(iterator->node, *output) > 0 )
			{
				*output = iterator->node;
			}
		}
		else // Find minimum
		{
			if ( cmp(iterator->node, *output) < 0 )
			{
				*output = iterator->node;
			}
		}
		iterator = iterator->next;
	}

	return LLIST_SUCCESS;
}

int llist_get_max(llist list, llist_node * max)
{
	return llist_get_min_max(list,max,true);
}

int llist_get_min(llist list, llist_node * min)
{
	return llist_get_min_max(list,min,false);
}

bool llist_is_empty(llist list)
{
    return ( ! llist_size ( list ) ) ;
}


/*
 * TODO: Implement the below functions
 */

int llist_merge ( llist first, llist second)
{
	assert (1 == 0); // Fail, function not implemented yet.
	return LLIST_NOT_IMPLEMENTED;
}






#include "hi_comm_ive.h"
#include "mpi_ive.h"
#include "mpi_sys.h"
#include <sys/time.h>
#include <time.h>
#include <pthread.h>



	HI_S32 s32Ret;
    HI_VOID *pVirtSrc = NULL;
	HI_VOID *pVirtDst = NULL;
	IVE_SRC_IMAGE_S input0_nv12;
	IVE_DST_IMAGE_S output0_rgbplanar_input1;
	IVE_DST_IMAGE_S output1_rgbplanar_input2;
	IVE_DST_IMAGE_S output2_nv12_input3;
	IVE_DST_IMAGE_S output3_rgbpackge;
    unsigned int * pImage = NULL;
    FILE* fp;
    struct timeval start,end,end0;
    IVE_HANDLE IveHandle;
    HI_BOOL bInstant = HI_TRUE;
       IVE_CSC_CTRL_S stCscCtrl0;
       IVE_RESIZE_CTRL_S stResizeCtrl1;
       IVE_CSC_CTRL_S stCscCtrl2;
       IVE_CSC_CTRL_S stCscCtrl3;
      char Internal_buffer[1920*1080*3];
      pthread_t process_thread_id;
      int process_thread_loop=0;

      llist inqueue_full,inqueue_empty;
      llist outqueue_full,outqueue_empty;
#define QUEUE_SIZE 3

      void *convert_thread(void);
int convert_init(int in_width,int in_height,int out_width,int out_height)
{
	  input0_nv12.enType = IVE_IMAGE_TYPE_YUV420SP;
	    input0_nv12.u32Width = in_width;
	    input0_nv12.u32Height = in_height;
	    input0_nv12.au32Stride[0]=input0_nv12.au32Stride[1]=input0_nv12.au32Stride[2]=input0_nv12.u32Width;


	    output0_rgbplanar_input1.enType = IVE_IMAGE_TYPE_U8C3_PLANAR;
	    output0_rgbplanar_input1.u32Width = in_width;
	    output0_rgbplanar_input1.u32Height = in_height;
	    output0_rgbplanar_input1.au32Stride[0]=output0_rgbplanar_input1.au32Stride[1]=output0_rgbplanar_input1.au32Stride[2]=output0_rgbplanar_input1.u32Width;

	    output1_rgbplanar_input2.enType = IVE_IMAGE_TYPE_U8C3_PLANAR;
	    output1_rgbplanar_input2.u32Width = out_width;
	    output1_rgbplanar_input2.u32Height = out_height;
	    output1_rgbplanar_input2.au32Stride[0]=output1_rgbplanar_input2.au32Stride[1]=output1_rgbplanar_input2.au32Stride[2]=output1_rgbplanar_input2.u32Width;

	    output2_nv12_input3.enType = IVE_IMAGE_TYPE_YUV420SP;
	    output2_nv12_input3.u32Width = out_width;
	    output2_nv12_input3.u32Height = out_height;
	    output2_nv12_input3.au32Stride[0]=output2_nv12_input3.au32Stride[1]=output2_nv12_input3.au32Stride[2]=output2_nv12_input3.u32Width;

	    output3_rgbpackge.enType = IVE_IMAGE_TYPE_U8C3_PACKAGE;
	    output3_rgbpackge.u32Width = out_width;
	    output3_rgbpackge.u32Height = out_height;
	    output3_rgbpackge.au32Stride[0]=output3_rgbpackge.au32Stride[1]=output3_rgbpackge.au32Stride[2]=output3_rgbpackge.u32Width;



	    stCscCtrl0.enMode=IVE_CSC_MODE_PIC_BT601_YUV2RGB;

	    stResizeCtrl1.enMode=IVE_RESIZE_MODE_LINEAR;
	    stResizeCtrl1.stMem.u32Size=in_width*in_height*3;
	    stResizeCtrl1.u16Num=1;


	    s32Ret = HI_MPI_SYS_MmzAlloc_Cached(&stResizeCtrl1.stMem.u64PhyAddr,(HI_VOID** )&stResizeCtrl1.stMem.u64VirAddr,"stResizeCtrl1", HI_NULL, stResizeCtrl1.stMem.u32Size);

	    if(s32Ret==HI_FAILURE)
	    {
	        printf("%s %d\n",__FUNCTION__,__LINE__);
	        return HI_FAILURE;
	    }

	    stCscCtrl2.enMode=IVE_CSC_MODE_VIDEO_BT601_RGB2YUV;

	    stCscCtrl3.enMode=IVE_CSC_MODE_PIC_BT601_YUV2RGB;


	    s32Ret = HI_MPI_SYS_MmzAlloc_Cached (&input0_nv12.au64PhyAddr[0],(HI_VOID** )&input0_nv12.au64VirAddr[0],"input0_nv12", HI_NULL, input0_nv12.u32Height *input0_nv12.u32Width*3);



	    if(s32Ret==HI_FAILURE)
	    {
	        printf("%s %d\n",__FUNCTION__,__LINE__);
	        return HI_FAILURE;
	    }
	    input0_nv12.au64PhyAddr[1]= input0_nv12.au64PhyAddr[0]+input0_nv12.u32Height * input0_nv12.au32Stride[0];


	    s32Ret = HI_MPI_SYS_MmzAlloc_Cached(&output0_rgbplanar_input1.au64PhyAddr[0],&output0_rgbplanar_input1.au64VirAddr[0],"output0_rgbplanar_input1", HI_NULL, output0_rgbplanar_input1.u32Height * output0_rgbplanar_input1.u32Width*3);
	    if(s32Ret==HI_FAILURE)
	    {
	        printf("%s %d\n",__FUNCTION__,__LINE__);
	        return HI_FAILURE;
	    }
	    output0_rgbplanar_input1.au64PhyAddr[1]= output0_rgbplanar_input1.au64PhyAddr[0]+output0_rgbplanar_input1.u32Height * output0_rgbplanar_input1.au32Stride[0];
	    output0_rgbplanar_input1.au64PhyAddr[2]= output0_rgbplanar_input1.au64PhyAddr[1]+output0_rgbplanar_input1.u32Height * output0_rgbplanar_input1.au32Stride[1];


	    s32Ret = HI_MPI_SYS_MmzAlloc_Cached(&output1_rgbplanar_input2.au64PhyAddr[0],&output1_rgbplanar_input2.au64VirAddr[0],"output1_rgbplanar_input2", HI_NULL, output1_rgbplanar_input2.u32Height * output1_rgbplanar_input2.u32Width*3);
	    if(s32Ret==HI_FAILURE)
	    {
	        printf("%s %d\n",__FUNCTION__,__LINE__);
	        return HI_FAILURE;
	    }
	    output1_rgbplanar_input2.au64PhyAddr[1]= output1_rgbplanar_input2.au64PhyAddr[0]+output1_rgbplanar_input2.u32Height * output1_rgbplanar_input2.au32Stride[0];
	    output1_rgbplanar_input2.au64PhyAddr[2]= output1_rgbplanar_input2.au64PhyAddr[1]+output1_rgbplanar_input2.u32Height * output1_rgbplanar_input2.au32Stride[1];


	    s32Ret = HI_MPI_SYS_MmzAlloc_Cached(&output2_nv12_input3.au64PhyAddr[0],&output2_nv12_input3.au64VirAddr[0],"output2_nv12_input3", HI_NULL, output2_nv12_input3.u32Height * output2_nv12_input3.u32Width*3);
	    if(s32Ret==HI_FAILURE)
	    {
	        printf("%s %d\n",__FUNCTION__,__LINE__);
	        return HI_FAILURE;
	    }
	    output2_nv12_input3.au64PhyAddr[1]= output2_nv12_input3.au64PhyAddr[0]+output2_nv12_input3.u32Height * output2_nv12_input3.au32Stride[0];



	    s32Ret = HI_MPI_SYS_MmzAlloc_Cached(&output3_rgbpackge.au64PhyAddr[0],&output3_rgbpackge.au64VirAddr[0],"output3_rgbpackge", HI_NULL, output3_rgbpackge.u32Height * output3_rgbpackge.u32Width*3);
	    if(s32Ret==HI_FAILURE)
	    {
	        printf("%s %d\n",__FUNCTION__,__LINE__);
	        return HI_FAILURE;
	    }
	    output3_rgbpackge.au64PhyAddr[1]= output3_rgbpackge.au64PhyAddr[0]+output3_rgbpackge.u32Height * output3_rgbpackge.au32Stride[0];
	    output3_rgbpackge.au64PhyAddr[2]= output3_rgbpackge.au64PhyAddr[1]+output3_rgbpackge.u32Height * output3_rgbpackge.au32Stride[1];


	    char *p;

	    inqueue_empty=llist_queue_init();
	    inqueue_full=llist_queue_init();
	    outqueue_empty=llist_queue_init();
	    outqueue_full=llist_queue_init();
	    for(int i=0;i<QUEUE_SIZE;i++){
	    	p=malloc(in_width*in_height*3);
	    	if(p){
	    		llist_queue(inqueue_empty,p);
	    	}else{
	    		printf("ERROR %s %d\n",__FUNCTION__,__LINE__);
	    		return -1;
	    	}

	    	p=malloc(out_width*out_height*3);
	    	if(p){
	    		llist_queue(outqueue_empty,p);
	    	}else{
	    		printf("ERROR %s %d\n",__FUNCTION__,__LINE__);
	    		return -1;
	    	}
	    }

	    pthread_create(&process_thread_id,NULL,(void*)convert_thread,NULL);


}

int convert_deinit()
{
    process_thread_loop=0;
    pthread_join(process_thread_id,0);

    HI_MPI_SYS_MmzFree(input0_nv12.au64PhyAddr[0], &input0_nv12.au64VirAddr[0]);
    HI_MPI_SYS_MmzFree(output0_rgbplanar_input1.au64PhyAddr[0], &output0_rgbplanar_input1.au64VirAddr[0]);
    HI_MPI_SYS_MmzFree(stResizeCtrl1.stMem.u64PhyAddr, &stResizeCtrl1.stMem.u64VirAddr);
    HI_MPI_SYS_MmzFree(output1_rgbplanar_input2.au64PhyAddr[0], &output1_rgbplanar_input2.au64VirAddr[0]);
    HI_MPI_SYS_MmzFree(output2_nv12_input3.au64PhyAddr[0], &output2_nv12_input3.au64VirAddr[0]);
    HI_MPI_SYS_MmzFree(output3_rgbpackge.au64PhyAddr[0], &output3_rgbpackge.au64VirAddr[0]);


    char *p;

    for(int i=0;i<QUEUE_SIZE;i++){
    		p=llist_dequeue(inqueue_empty);
    		if(p)
    			free(p);
    		p=llist_dequeue(inqueue_full);
    		if(p)
    			free(p);
    		p=llist_dequeue(outqueue_empty);
    		if(p)
    			free(p);
    		p=llist_dequeue(outqueue_full);
    		if(p)
    			free(p);
    }
}
int convert_process(char *inbuffer,char *outbuffer)
{



    memcpy((HI_VOID *)input0_nv12.au64VirAddr[0],inbuffer,input0_nv12.u32Height*input0_nv12.au32Stride[0]*3/2);

    HI_MPI_SYS_MmzFlushCache(input0_nv12.au64PhyAddr[0],(HI_VOID *)input0_nv12.au64VirAddr[0],input0_nv12.u32Height*input0_nv12.au32Stride[0]*3/2);




    HI_BOOL block=HI_TRUE;
    HI_BOOL finished=HI_FALSE;


    s32Ret=HI_MPI_IVE_CSC(&IveHandle,&input0_nv12,&output0_rgbplanar_input1,&stCscCtrl0,bInstant);
    if(s32Ret==HI_FAILURE)
    {
        printf("%s %d\n",__FUNCTION__,__LINE__);
        return HI_FAILURE;
    }

    s32Ret=HI_MPI_IVE_Resize(&IveHandle,&output0_rgbplanar_input1,&output1_rgbplanar_input2,&stResizeCtrl1,bInstant);
    if(s32Ret==HI_FAILURE)
    {
        printf("%s %d\n",__FUNCTION__,__LINE__);
        return HI_FAILURE;
    }
#if 1
    s32Ret= HI_MPI_IVE_Query(IveHandle,&finished,block);
    if(s32Ret==HI_FAILURE)
    {
        printf("%s %d\n",__FUNCTION__,__LINE__);
        return HI_FAILURE;
    }
    HI_MPI_SYS_MmzFlushCache(output1_rgbplanar_input2.au64PhyAddr[0],(HI_VOID *)output1_rgbplanar_input2.au64VirAddr[0],output1_rgbplanar_input2.u32Height*output1_rgbplanar_input2.au32Stride[0]*3);
    output1_rgbplanar_input2.au64VirAddr[1]=output1_rgbplanar_input2.au64VirAddr[0]+output1_rgbplanar_input2.u32Height*output1_rgbplanar_input2.au32Stride[0];
    output1_rgbplanar_input2.au64VirAddr[2]=output1_rgbplanar_input2.au64VirAddr[1]+output1_rgbplanar_input2.u32Height*output1_rgbplanar_input2.au32Stride[1];


    memcpy(Internal_buffer,output1_rgbplanar_input2.au64VirAddr[0],output1_rgbplanar_input2.u32Height*output1_rgbplanar_input2.au32Stride[0]);
    memcpy(output1_rgbplanar_input2.au64VirAddr[0],output1_rgbplanar_input2.au64VirAddr[2],output1_rgbplanar_input2.u32Height*output1_rgbplanar_input2.au32Stride[0]);
    memcpy(output1_rgbplanar_input2.au64VirAddr[2],Internal_buffer,output1_rgbplanar_input2.u32Height*output1_rgbplanar_input2.au32Stride[0]);

    HI_MPI_SYS_MmzFlushCache(output1_rgbplanar_input2.au64PhyAddr[0],(HI_VOID *)output1_rgbplanar_input2.au64VirAddr[0],output1_rgbplanar_input2.u32Height*output1_rgbplanar_input2.au32Stride[0]*3);
#endif




    s32Ret=HI_MPI_IVE_CSC(&IveHandle,&output1_rgbplanar_input2,&output2_nv12_input3,&stCscCtrl2,bInstant);
    if(s32Ret==HI_FAILURE)
    {
        printf("%s %d\n",__FUNCTION__,__LINE__);
        return HI_FAILURE;
    }





    s32Ret=HI_MPI_IVE_CSC(&IveHandle,&output2_nv12_input3,&output3_rgbpackge,&stCscCtrl3,bInstant);
    if(s32Ret==HI_FAILURE)
    {
        printf("%s %d\n",__FUNCTION__,__LINE__);
        return HI_FAILURE;
    }


    s32Ret= HI_MPI_IVE_Query(IveHandle,&finished,block);
    if(s32Ret==HI_FAILURE)
    {
        printf("%s %d\n",__FUNCTION__,__LINE__);
        return HI_FAILURE;
    }




 //   HI_MPI_SYS_MmzFlushCache(stDst.au64PhyAddr[0],(HI_VOID *)stDst.au64VirAddr[0],stDst.u32Height*stDst.au32Stride[0]*3);

//   	rgb_planar2package(stDst.u32Width,stDst.u32Height,stDst.au64VirAddr[0],buffer);


//    	fp= fopen("test00.rgb","w");;
//    	fwrite(stDst.au64VirAddr[0],1,stDst.u32Height*stDst.u32Width*3,fp);
//    	fclose(fp);
//    	fp= fopen("test01.rgb","w");;
//      	fwrite(buffer,1,stDst.u32Height*stDst.u32Width*3,fp);
//    	fclose(fp);


     HI_MPI_SYS_MmzFlushCache(output3_rgbpackge.au64PhyAddr[0],(HI_VOID *)output3_rgbpackge.au64VirAddr[0],output3_rgbpackge.u32Height*output3_rgbpackge.au32Stride[0]*3);


     memcpy(outbuffer,output3_rgbpackge.au64VirAddr[0],output3_rgbpackge.u32Height*output3_rgbpackge.u32Width*3);


    //uninit();
}



int convert_queue(char *inbuffer)
{
	char *p;

	p=llist_dequeue(inqueue_empty);
	if(p){

	}else{
		p=llist_dequeue(inqueue_full);
		printf("%s %d dequeue full queue\n",__FUNCTION__,__LINE__);
	}

	memcpy(p,inbuffer,input0_nv12.u32Width*input0_nv12.u32Height*3/2);
	llist_queue(inqueue_full,p);
}


int convert_dequeue(char *outbuffer)
{
	char *p;
	int ret=0;

	p=llist_dequeue(outqueue_full);
	if(p){
		ret=output3_rgbpackge.u32Height*output3_rgbpackge.u32Width*3;
		memcpy(outbuffer,p,ret);
		llist_queue(outqueue_empty,p);
	}

	return ret;
}


void *convert_thread(void)

{

   process_thread_loop=1;
   char *inbuffer,*outbuffer;

   while(process_thread_loop){

	   inbuffer=llist_dequeue(inqueue_full);

	   if(!inbuffer){
		   usleep(5*1000);
		   continue;
	   }

	   outbuffer=llist_dequeue(outqueue_empty);
	   if(outbuffer){

	   	}else{
	   		outbuffer=llist_dequeue(outqueue_full);
	   		//printf("%s %d dequeue full queue\n",__FUNCTION__,__LINE__);
	   	}

	   convert_process(inbuffer,outbuffer);

	   llist_queue(inqueue_empty,inbuffer);
	   llist_queue(outqueue_full,outbuffer);

   }

}

//int main()
//{
//	   int in_width=1920;
//	   int in_height=1080;
//	   int out_width=704;//out_width%16=0
//	   int out_height=576;//out_height%2=0
//
//		char *buffer=malloc(in_width*in_height*3);
//
//		csc_init(in_width,in_height,out_width,out_height);
//
//	  fp=fopen("nv21.yuv","r");
//
//	    if(fp==NULL)
//	    {
//	        printf("%s %d\n",__FUNCTION__,__LINE__);
//	        return -1;
//	    }
//
//
//	    fread( buffer,1,in_width*in_height*3/2,fp);
//	    printf("%s %d\n",__FUNCTION__,__LINE__);
//	    fclose(fp);
//
//
//
//
//		gettimeofday(&start, NULL);
//		csc_process(buffer,buffer);
//		gettimeofday(&end, NULL);
//		printf("%d time=%lu\n",__LINE__,end.tv_sec*1000+end.tv_usec/1000-start.tv_sec*1000-start.tv_usec/1000 );
//
//
//
//	  	fp= fopen("test11.rgb","w");;
//	  	fwrite(buffer,1,out_width*out_height*3,fp);
//	  	fclose(fp);
//
//	  	free(buffer);
//
//	  	csc_deinit();
//
//
//	      printf("done!\n");
//}

