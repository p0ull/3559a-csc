/*
 * test.c
 *
 *  Created on: 2018Äê7ÔÂ6ÈÕ
 *      Author: admin
 */



#include "hi_comm_ive.h"
#include "mpi_ive.h"
#include "mpi_sys.h"




#include <sys/time.h>
#include <time.h>




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



int csc_init(int in_width,int in_height,int out_width,int out_height)
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
}

int convert_deinit(){

    HI_MPI_SYS_MmzFree(input0_nv12.au64PhyAddr[0], &input0_nv12.au64VirAddr[0]);
    HI_MPI_SYS_MmzFree(output0_rgbplanar_input1.au64PhyAddr[0], &output0_rgbplanar_input1.au64VirAddr[0]);
    HI_MPI_SYS_MmzFree(stResizeCtrl1.stMem.u64PhyAddr, &stResizeCtrl1.stMem.u64VirAddr);
    HI_MPI_SYS_MmzFree(output1_rgbplanar_input2.au64PhyAddr[0], &output1_rgbplanar_input2.au64VirAddr[0]);
    HI_MPI_SYS_MmzFree(output2_nv12_input3.au64PhyAddr[0], &output2_nv12_input3.au64VirAddr[0]);
    HI_MPI_SYS_MmzFree(output3_rgbpackge.au64PhyAddr[0], &output3_rgbpackge.au64VirAddr[0]);
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
