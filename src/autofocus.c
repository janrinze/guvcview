/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Dr. Alexander K. Seewald <alex@seewald.at>                          #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

/*******************************************************************************#
#                                                                               #
#  autofocus - using dct                                                        #
#                                                                               # 
# 							                        #
********************************************************************************/

#include "autofocus.h"
#include "dct.h"
#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SORT (2) //1 - Quick sort   2 - Shell sort  other - bubble sort

#define TH		(80) // treshold = 1/80 of focus sharpness value

#define FLAT 		(0)
#define LOCAL_MAX	(1)
#define LEFT		(2)
#define RIGHT		(3)
#define INCSTEP		(4)

static double sumAC[64];
static int ACweight[64] = {
	0,1,2,3,4,5,6,7,
	1,1,2,3,4,5,6,7,
	2,2,2,3,4,5,6,7,
	3,3,3,3,4,5,6,7,
	4,4,4,4,4,5,6,7,
	5,5,5,5,5,5,6,7,
	7,7,7,7,7,7,7,7
};

void initFocusData (struct focusData *AFdata) {
	memset(AFdata,0,sizeof(struct focusData));
    	AFdata->right=255;
	AFdata->left=8; /*start with focus at 8*/
	AFdata->focus=-1;
	AFdata->focus_wait=0;
	memset(sumAC,0,64);
    	/*all other values are 0 */
}

#if (SORT == 1)
/*quick sort (the fastest and more complex - recursive)*/
static void q_sort(struct focusData *AFdata, int left, int right)
{
	int pivot, l_hold, r_hold, temp;

	l_hold = left;
	r_hold = right;
	pivot = AFdata->arr_sharp[left];
	temp = AFdata->arr_foc[left];
	
	while(left < right)
	{
		while((AFdata->arr_sharp[right] >= pivot) && (left < right))
			right--;
		if (left != right)
		{
			AFdata->arr_sharp[left] = AFdata->arr_sharp[right];
			AFdata->arr_foc[left] = AFdata->arr_foc[right];
			left++;
		}
		while((AFdata->arr_sharp[left] <= pivot) && (left < right))
			left++;
		if (left != right)
		{
			AFdata->arr_sharp[right] = AFdata->arr_sharp[left];
			AFdata->arr_foc[right] = AFdata->arr_foc[left];
			right--;
		}
	}
	AFdata->arr_sharp[left] = pivot;
	AFdata->arr_foc[left] = temp;
	pivot = left;
	left = l_hold;
	right = r_hold;
	if (left < pivot)
	q_sort(AFdata, left, pivot-1);
	if (right > pivot)
	q_sort(AFdata, pivot+1, right);

}
#elif (SORT == 2)  
/* shell sort (simpler and around 5 times faster than bubble sort)*/
void s_sort(struct focusData *AFdata, int size)
{
	int i, j, increment, temp, tempf;

	increment = size/2;
	while (increment > 0)
	{
	for (i=increment; i <= size; i++)
	{
		j = i;
		temp = AFdata->arr_sharp[i];
		tempf = AFdata->arr_foc[i];
		while ((j >= increment) && (AFdata->arr_sharp[j-increment] > temp))
		{
			AFdata->arr_sharp[j] = AFdata->arr_sharp[j - increment];
			AFdata->arr_foc[j] = AFdata->arr_foc[j - increment];
			j = j - increment;
		}
		AFdata->arr_sharp[j] = temp;
		AFdata->arr_foc[j] = tempf;
	}
	if (increment == 2)
		increment = 1;
	else
		increment =(int) (increment / 2.2);
	}
}

#else
/*buble sort (the simplest and most inefficient)*/
static void b_sort (struct focusData *AFdata, int size) 
{
	int swapped = 0;
	int temp=0;
	int i;
	do {
		swapped = 0;
		size--;
		for (i=0;i<=size;i++) {
			if (AFdata->arr_sharp[i+1] > AFdata->arr_sharp[i]) {
				temp = AFdata->arr_sharp[i+1];
				AFdata->arr_sharp[i+1]=AFdata->arr_sharp[i];
				AFdata->arr_sharp[i]=temp;
				temp=AFdata->arr_foc[i+1];
				AFdata->arr_foc[i+1]=AFdata->arr_foc[i];
				AFdata->arr_foc[i]=temp;
				swapped = 1;
			}
		}
	 } while (swapped);
}
#endif

static int Sort(struct focusData *AFdata, int size)
{
	if (size>=20) {
		printf("WARNING: focus array size=%d exceeds 20\n",size);
		size = 10;
	}
#if (SORT == 1)
	q_sort(AFdata, 0, size);
#elif (SORT == 2)
	s_sort(AFdata, size);
#else
	b_sort(AFdata, size);
#endif
	
	/*better focus value*/ 
	return(AFdata->arr_foc[size]);
}

/* extract lum (y) data from image    (YUYV)                */
/* img - image data pointer                                 */
/* dataY - pointer for lum (y) data                         */
/* width - width of img (in pixels)                         */
/* height - height of img (in pixels)                       */
static INT16* extractY (BYTE* img, INT16* dataY, int width, int height) {
	int i=0;
    	BYTE *pimg;
    	pimg=img;
    	
    	for (i=0;i<(height*width);i++) {
		dataY[i]=(INT16) *pimg++;
		pimg++;
	}
	
	return (dataY);
}

/* measure sharpness in MCU                 */
/* data - MCU data [8x8]                    */
/* t - highest order coef.                  */
static void getSharpnessMCU (INT16 *data, double weight) {

	int i=0;
	int j=0;
		

	levelshift (data);
	DCT (data);

	for (i=0;i<8;i++) {
		for(j=0;j<8;j++) {
			sumAC[i*8+j]+=data[i*8+j]*data[i*8+j]*weight;
		}	
	}	
}

/* sharpness in focus window */
int getSharpness (BYTE* img, int width, int height, int t) {

	float res=0;
	int numMCUx = width/(8*2); /*covers 1/2 of width - width should be even*/
    	int numMCUy = height/(8*2); /*covers 1/2 of height- height should be even*/
	INT16 dataMCU[64];
    	INT16* data;
    	INT16 dataY[width*height];
	INT16 *Y = dataY;
	
	double weight;
	double xp_;
	double yp_;
	int ctx = numMCUx >> 1; /*center*/ 
	int cty = numMCUy >> 1;
	double rad=ctx/2; 
	if (cty<ctx) { rad=cty/2; }
	rad=rad*rad;
    	int cnt2 =0;
	
	data=dataMCU;
    
	Y = extractY (img, Y, width, height);
	
    	int i=0;
    	int j=0;
    	int xp=0;
    	int yp=0;
	/*calculate MCU sharpness*/
	for (yp=0;yp<numMCUy;yp++) {
		yp_=yp-cty;
    		for (xp=0;xp<numMCUx;xp++) {
			xp_=xp-ctx;
			weight = exp(-(xp_*xp_)/rad-(yp_*yp_)/rad);
			for (i=0;i<8;i++) {
				for(j=0;j<8;j++) {
				    	/*center*/
					dataMCU[i*8+j]=Y[(((height-(numMCUy-(yp*2))*8)>>1)+i)*width+(((width-(numMCUx-(xp*2))*8)>>1)+j)];
				}
			}
			getSharpnessMCU(data,weight);
			cnt2++;
		}
	}
    	
	
	for (i=0;i<=t;i++) {
		for(j=0;j<t;j++) {
			sumAC[i*8+j]/=(double) (cnt2); /*average = mean*/
			res+=sumAC[i*8+j]*ACweight[i*8+j];
		}
	}
	return (roundf(res*10)); /*round to int (4 digit precision)*/
}


static int checkFocus(struct focusData *AFdata) {
	
    if (AFdata->step<=8) {
	if (abs((AFdata->sharpLeft-AFdata->focus_sharpness)<(AFdata->focus_sharpness/TH)) && 
	    (abs(AFdata->sharpRight-AFdata->focus_sharpness)<(AFdata->focus_sharpness/TH))) {
		return (FLAT);
	} else if (((AFdata->focus_sharpness-AFdata->sharpRight))>=(AFdata->focus_sharpness/TH) && 
	    ((AFdata->focus_sharpness-AFdata->sharpLeft))>=(AFdata->focus_sharpness/TH)) {
		// significantly down in both directions -> check another step
		// outside for local maximum
		AFdata->step=16;
		return (INCSTEP);
	} else {
		// one is significant, the other is not...
		int left=0; int right=0;
		if (abs((AFdata->sharpLeft-AFdata->focus_sharpness))>=(AFdata->focus_sharpness/TH)) {
			if (AFdata->sharpLeft>AFdata->focus_sharpness) left++;  
			else right++; 
		}
		if (abs((AFdata->sharpRight-AFdata->focus_sharpness))>=(AFdata->focus_sharpness/TH)) {
			if (AFdata->sharpRight>AFdata->focus_sharpness) right++; 
			else left++;
		}
		if (left==right) return (FLAT);
		else if (left>right) return (LEFT);
		else return (RIGHT);
	}
	
    } else {
        if (((AFdata->focus_sharpness-AFdata->sharpRight))>=(AFdata->focus_sharpness/TH) && 
           ((AFdata->focus_sharpness-AFdata->sharpLeft))>=(AFdata->focus_sharpness/TH)) {
		return (LOCAL_MAX);
        } else {
		return (FLAT);
        }
    }
    
}

int getFocusVal (struct focusData *AFdata) {
    	int step = 20;
    	int step2 = 2;
	int focus=0;
	
	switch (AFdata->flag) {
	    /*--------- first time - run sharpness algorithm -----------------*/
	    if(AFdata->ind >= 20) {
		printf ("WARNING ind=%d exceeds 20\n",AFdata->ind);
		AFdata->ind = 10;
	    }
	    case 0: /*sample left to right*/
	    	AFdata->arr_sharp[AFdata->ind] = AFdata->sharpness;
		AFdata->arr_foc[AFdata->ind] = AFdata->focus;
		if (AFdata->focus > (AFdata->right - step)) { /*get left and right from arr_sharp*/	
			focus=Sort(AFdata,AFdata->ind);
		       	/*get a window around the best value*/
			AFdata->left = (focus- step/2);
			AFdata->right = (focus + step/2);
			if (AFdata->left < 0) AFdata->left=0;
			if (AFdata->right > 255) AFdata->right=255;
			AFdata->focus = AFdata->left;
		    	AFdata->ind=0;
			AFdata->flag = 1;
		} else { 
		    	AFdata->focus=AFdata->arr_foc[AFdata->ind] + step; /*next focus*/
			AFdata->ind=AFdata->ind+1;;
			AFdata->flag = 0;
		}
		
	    	break;
	    case 1: /*sample left to right*/ 
	    	AFdata->arr_sharp[AFdata->ind] = AFdata->sharpness;
		AFdata->arr_foc[AFdata->ind] = AFdata->focus;
		if (AFdata->focus > (AFdata->right - step2)) { /*get left and right from arr_sharp*/	
			focus=Sort(AFdata,AFdata->ind);
		       	/*get the best value*/
			AFdata->focus = focus;
			AFdata->focus_sharpness = AFdata->arr_sharp[AFdata->ind];
			AFdata->step = 8; /*first step for focus tracking*/
			AFdata->focusDir = FLAT; /*no direction for focus*/
			AFdata->flag = 2;
		} else { 
		    	AFdata->focus=AFdata->arr_foc[AFdata->ind] + step2; /*next focus*/
			AFdata->ind=AFdata->ind+1;;
			AFdata->flag = 1;
		}
		
	    	break;
	    case 2: /* set treshold in order to sharpness*/
		if (AFdata->setFocus) {
			AFdata->setFocus = 0;
		    	AFdata->flag= 0;
		    	AFdata->right = 255;
		    	AFdata->left = 8;
		    	AFdata->ind = 0;
		} else {
		    /*track focus*/
		    AFdata->focus_sharpness=AFdata->sharpness;
		    AFdata->flag= 3;
		    AFdata->sharpLeft=0;
		    AFdata->sharpRight=0;
		    AFdata->focus+=AFdata->step; /*check right*/
		}
	    	break;
	    case 3:
		/*track focus*/
		AFdata->flag= 4;
		AFdata->sharpRight=AFdata->sharpness;
		AFdata->focus-=(2*AFdata->step); /*check left*/
		break;
	    case 4:
		/*track focus*/
		AFdata->sharpLeft=AFdata->sharpness;
		int ret=0;
		ret=checkFocus(AFdata);
		//printf("check is %d\n",ret);
		switch (ret) {
			case LOCAL_MAX:
			    AFdata->focus += AFdata->step; /*return to orig. focus*/
			    AFdata->step = 8;
			    AFdata->flag = 2;
			    break;
			case FLAT:
			    if(AFdata->focusDir == FLAT) {
				AFdata->focus += AFdata->step; /*return to orig. focus*/
				AFdata->step = 8;
				AFdata->flag = 2;
			    } else if (AFdata->focusDir == RIGHT) {
				AFdata->focus += 2*AFdata->step; /*go right*/
				AFdata->step = 8;
				AFdata->flag = 2;
			    } else { /*go left*/
				AFdata->step = 8;
				AFdata->flag = 2;
			    }
			    break;
			case RIGHT:
			    AFdata->focus += 2*AFdata->step; /*go right*/
			    AFdata->flag = 2;
			    break;
			case LEFT:
			    /*keep focus on left*/
			    AFdata->flag = 2;
			    break;
			case INCSTEP:
			    AFdata->focus += AFdata->step; /*return to orig. focus*/
			    AFdata->step = 16;
			    AFdata->flag = 2;
			    break;
		}
		break;
		  
	}	
    	
    	/*clip focus, right and left*/
    	AFdata->focus=(AFdata->focus>255)?255:((AFdata->focus<0)?0:AFdata->focus);
	AFdata->right=(AFdata->right>255)?255:((AFdata->right<0)?0:AFdata->right);
    	AFdata->left=(AFdata->left>255)?255:((AFdata->left<0)?0:AFdata->left);
    
	return AFdata->focus;
}
