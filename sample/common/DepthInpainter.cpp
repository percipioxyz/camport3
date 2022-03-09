#include "DepthInpainter.hpp"
#include <stdint.h>

#include <opencv2/opencv.hpp>
#ifndef CV_VERSION_EPOCH
#if defined (CV_MAJOR_VERSION) && (CV_VERSION_MAJOR == 4)
#include <opencv2/imgproc/types_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/photo/legacy/constants_c.h>
#include <opencv2/imgcodecs/legacy/constants_c.h>
#endif
#endif

using namespace  cv;

#undef CV_MAT_ELEM_PTR_FAST
#define CV_MAT_ELEM_PTR_FAST( mat, row, col, pix_size )  \
     ((mat).data.ptr + (size_t)(mat).step*(row) + (pix_size)*(col))

inline float
min4( float a, float b, float c, float d )
{
    a = MIN(a,b);
    c = MIN(c,d);
    return MIN(a,c);
}

#define CV_MAT_3COLOR_ELEM(img,type,y,x,c) CV_MAT_ELEM(img,type,y,(x)*3+(c))
#define KNOWN  0  //known outside narrow band
#define BAND   1  //narrow band (known)
#define INSIDE 2  //unknown
#define CHANGE 3  //servise

typedef struct CvHeapElem
{
    float T;
    int i,j;
    struct CvHeapElem* prev;
    struct CvHeapElem* next;
}
CvHeapElem;


class CvPriorityQueueFloat
{
protected:
    CvHeapElem *mem,*empty,*head,*tail;
    int num,in;

public:
    bool Init( const CvMat* f )
    {
        int i,j;
        for( i = num = 0; i < f->rows; i++ )
        {
            for( j = 0; j < f->cols; j++ )
                num += CV_MAT_ELEM(*f,uchar,i,j)!=0;
        }
        if (num<=0) return false;
        mem = (CvHeapElem*)cvAlloc((num+2)*sizeof(CvHeapElem));
        if (mem==NULL) return false;

        head       = mem;
        head->i    = head->j = -1;
        head->prev = NULL;
        head->next = mem+1;
        head->T    = -FLT_MAX;
        empty      = mem+1;
        for (i=1; i<=num; i++) {
            mem[i].prev   = mem+i-1;
            mem[i].next   = mem+i+1;
            mem[i].i      = -1;
            mem[i].T      = FLT_MAX;
        }
        tail       = mem+i;
        tail->i    = tail->j = -1;
        tail->prev = mem+i-1;
        tail->next = NULL;
        tail->T    = FLT_MAX;
        return true;
    }

    bool Add(const CvMat* f) {
        int i,j;
        for (i=0; i<f->rows; i++) {
            for (j=0; j<f->cols; j++) {
                if (CV_MAT_ELEM(*f,uchar,i,j)!=0) {
                    if (!Push(i,j,0)) return false;
                }
            }
        }
        return true;
    }

    bool Push(int i, int j, float T) {
        CvHeapElem *tmp=empty,*add=empty;
        if (empty==tail) return false;
        while (tmp->prev->T>T) tmp = tmp->prev;
        if (tmp!=empty) {
            add->prev->next = add->next;
            add->next->prev = add->prev;
            empty = add->next;
            add->prev = tmp->prev;
            add->next = tmp;
            add->prev->next = add;
            add->next->prev = add;
        } else {
            empty = empty->next;
        }
        add->i = i;
        add->j = j;
        add->T = T;
        in++;
        //      printf("push i %3d  j %3d  T %12.4e  in %4d\n",i,j,T,in);
        return true;
    }

    bool Pop(int *i, int *j) {
        CvHeapElem *tmp=head->next;
        if (empty==tmp) return false;
        *i = tmp->i;
        *j = tmp->j;
        tmp->prev->next = tmp->next;
        tmp->next->prev = tmp->prev;
        tmp->prev = empty->prev;
        tmp->next = empty;
        tmp->prev->next = tmp;
        tmp->next->prev = tmp;
        empty = tmp;
        in--;
        //      printf("pop  i %3d  j %3d  T %12.4e  in %4d\n",tmp->i,tmp->j,tmp->T,in);
        return true;
    }

    bool Pop(int *i, int *j, float *T) {
        CvHeapElem *tmp=head->next;
        if (empty==tmp) return false;
        *i = tmp->i;
        *j = tmp->j;
        *T = tmp->T;
        tmp->prev->next = tmp->next;
        tmp->next->prev = tmp->prev;
        tmp->prev = empty->prev;
        tmp->next = empty;
        tmp->prev->next = tmp;
        tmp->next->prev = tmp;
        empty = tmp;
        in--;
        //      printf("pop  i %3d  j %3d  T %12.4e  in %4d\n",tmp->i,tmp->j,tmp->T,in);
        return true;
    }

    CvPriorityQueueFloat(void) {
        num=in=0;
        mem=empty=head=tail=NULL;
    }

    ~CvPriorityQueueFloat(void)
    {
        cvFree( &mem );
    }
};

inline float VectorScalMult(CvPoint2D32f v1,CvPoint2D32f v2) {
   return v1.x*v2.x+v1.y*v2.y;
}

inline float VectorLength(CvPoint2D32f v1) {
   return v1.x*v1.x+v1.y*v1.y;
}

///////////////////////////////////////////////////////////////////////////////////////////
//HEAP::iterator Heap_Iterator;
//HEAP Heap;

static float FastMarching_solve(int i1,int j1,int i2,int j2, const CvMat* f, const CvMat* t)
{
    double sol, a11, a22, m12;
    a11=CV_MAT_ELEM(*t,float,i1,j1);
    a22=CV_MAT_ELEM(*t,float,i2,j2);
    m12=MIN(a11,a22);

    if( CV_MAT_ELEM(*f,uchar,i1,j1) != INSIDE )
        if( CV_MAT_ELEM(*f,uchar,i2,j2) != INSIDE )
            if( fabs(a11-a22) >= 1.0 )
                sol = 1+m12;
            else
                sol = (a11+a22+sqrt((double)(2-(a11-a22)*(a11-a22))))*0.5;
        else
            sol = 1+a11;
    else if( CV_MAT_ELEM(*f,uchar,i2,j2) != INSIDE )
        sol = 1+a22;
    else
        sol = 1+m12;

    return (float)sol;
}

/////////////////////////////////////////////////////////////////////////////////////


static void
icvCalcFMM(const CvMat *f, CvMat *t, CvPriorityQueueFloat *Heap, bool negate) {
   int i, j, ii = 0, jj = 0, q;
   float dist;

   while (Heap->Pop(&ii,&jj)) {

      unsigned known=(negate)?CHANGE:KNOWN;
      CV_MAT_ELEM(*f,uchar,ii,jj) = (uchar)known;

      for (q=0; q<4; q++) {
         i=0; j=0;
         if     (q==0) {i=ii-1; j=jj;}
         else if(q==1) {i=ii;   j=jj-1;}
         else if(q==2) {i=ii+1; j=jj;}
         else {i=ii;   j=jj+1;}
         if ((i<=0)||(j<=0)||(i>f->rows)||(j>f->cols)) continue;

         if (CV_MAT_ELEM(*f,uchar,i,j)==INSIDE) {
            dist = min4(FastMarching_solve(i-1,j,i,j-1,f,t),
                        FastMarching_solve(i+1,j,i,j-1,f,t),
                        FastMarching_solve(i-1,j,i,j+1,f,t),
                        FastMarching_solve(i+1,j,i,j+1,f,t));
            CV_MAT_ELEM(*t,float,i,j) = dist;
            CV_MAT_ELEM(*f,uchar,i,j) = BAND;
            Heap->Push(i,j,dist);
         }
      }
   }

   if (negate) {
      for (i=0; i<f->rows; i++) {
         for(j=0; j<f->cols; j++) {
            if (CV_MAT_ELEM(*f,uchar,i,j) == CHANGE) {
               CV_MAT_ELEM(*f,uchar,i,j) = KNOWN;
               CV_MAT_ELEM(*t,float,i,j) = -CV_MAT_ELEM(*t,float,i,j);
            }
         }
      }
   }
}


static void
icvTeleaInpaintFMM(const CvMat *f, CvMat *t, CvMat *out, int range, CvPriorityQueueFloat *Heap ) {
   int i = 0, j = 0, ii = 0, jj = 0, k, l, q, color = 0;
   float dist;

   if (CV_MAT_CN(out->type)==1) {

      while (Heap->Pop(&ii,&jj)) {

         CV_MAT_ELEM(*f,uchar,ii,jj) = KNOWN;
         for(q=0; q<4; q++) {
            if     (q==0) {i=ii-1; j=jj;}
            else if(q==1) {i=ii;   j=jj-1;}
            else if(q==2) {i=ii+1; j=jj;}
            else if(q==3) {i=ii;   j=jj+1;}
            if ((i<=1)||(j<=1)||(i>t->rows-1)||(j>t->cols-1)) continue;

            if (CV_MAT_ELEM(*f,uchar,i,j)==INSIDE) {
               dist = min4(FastMarching_solve(i-1,j,i,j-1,f,t),
                           FastMarching_solve(i+1,j,i,j-1,f,t),
                           FastMarching_solve(i-1,j,i,j+1,f,t),
                           FastMarching_solve(i+1,j,i,j+1,f,t));
               CV_MAT_ELEM(*t,float,i,j) = dist;

               for (color=0; color<=0; color++) {
                  CvPoint2D32f gradI,gradT,r;
                  float Ia=0,Jx=0,Jy=0,s=1.0e-20f,w,dst,lev,dir,sat;

                  if (CV_MAT_ELEM(*f,uchar,i,j+1)!=INSIDE) {
                     if (CV_MAT_ELEM(*f,uchar,i,j-1)!=INSIDE) {
                        gradT.x=(float)((CV_MAT_ELEM(*t,float,i,j+1)-CV_MAT_ELEM(*t,float,i,j-1)))*0.5f;
                     } else {
                        gradT.x=(float)((CV_MAT_ELEM(*t,float,i,j+1)-CV_MAT_ELEM(*t,float,i,j)));
                     }
                  } else {
                     if (CV_MAT_ELEM(*f,uchar,i,j-1)!=INSIDE) {
                        gradT.x=(float)((CV_MAT_ELEM(*t,float,i,j)-CV_MAT_ELEM(*t,float,i,j-1)));
                     } else {
                        gradT.x=0;
                     }
                  }
                  if (CV_MAT_ELEM(*f,uchar,i+1,j)!=INSIDE) {
                     if (CV_MAT_ELEM(*f,uchar,i-1,j)!=INSIDE) {
                        gradT.y=(float)((CV_MAT_ELEM(*t,float,i+1,j)-CV_MAT_ELEM(*t,float,i-1,j)))*0.5f;
                     } else {
                        gradT.y=(float)((CV_MAT_ELEM(*t,float,i+1,j)-CV_MAT_ELEM(*t,float,i,j)));
                     }
                  } else {
                     if (CV_MAT_ELEM(*f,uchar,i-1,j)!=INSIDE) {
                        gradT.y=(float)((CV_MAT_ELEM(*t,float,i,j)-CV_MAT_ELEM(*t,float,i-1,j)));
                     } else {
                        gradT.y=0;
                     }
                  }
                  for (k=i-range; k<=i+range; k++) {
                     int km=k-1+(k==1),kp=k-1-(k==t->rows-2);
                     for (l=j-range; l<=j+range; l++) {
                        int lm=l-1+(l==1),lp=l-1-(l==t->cols-2);
                        if (k>0&&l>0&&k<t->rows-1&&l<t->cols-1) {
                           if ((CV_MAT_ELEM(*f,uchar,k,l)!=INSIDE)&&
                               ((l-j)*(l-j)+(k-i)*(k-i)<=range*range)) {
                              r.y     = (float)(i-k);
                              r.x     = (float)(j-l);

                              dst = (float)(1./(VectorLength(r)*sqrt(VectorLength(r))));
                              lev = (float)(1./(1+fabs(CV_MAT_ELEM(*t,float,k,l)-CV_MAT_ELEM(*t,float,i,j))));

                              dir=VectorScalMult(r,gradT);
                              if (fabs(dir)<=0.01) dir=0.000001f;
                              w = (float)fabs(dst*lev*dir);

                              if (CV_MAT_ELEM(*f,uchar,k,l+1)!=INSIDE) {
                                 if (CV_MAT_ELEM(*f,uchar,k,l-1)!=INSIDE) {
                                    // gradI.x=(float)((CV_MAT_ELEM(*out,uchar,km,lp+1)-CV_MAT_ELEM(*out,uchar,km,lm-1)))*2.0f;
                                    gradI.x=(float)((CV_MAT_ELEM(*out,uint16_t,km,lp+1)-CV_MAT_ELEM(*out,uint16_t,km,lm-1)))*2.0f;
                                 } else {
                                    // gradI.x=(float)((CV_MAT_ELEM(*out,uchar,km,lp+1)-CV_MAT_ELEM(*out,uchar,km,lm)));
                                    gradI.x=(float)((CV_MAT_ELEM(*out,uint16_t,km,lp+1)-CV_MAT_ELEM(*out,uint16_t,km,lm)));
                                 }
                              } else {
                                 if (CV_MAT_ELEM(*f,uchar,k,l-1)!=INSIDE) {
                                    // gradI.x=(float)((CV_MAT_ELEM(*out,uchar,km,lp)-CV_MAT_ELEM(*out,uchar,km,lm-1)));
                                    gradI.x=(float)((CV_MAT_ELEM(*out,uint16_t,km,lp)-CV_MAT_ELEM(*out,uint16_t,km,lm-1)));
                                 } else {
                                    gradI.x=0;
                                 }
                              }
                              if (CV_MAT_ELEM(*f,uchar,k+1,l)!=INSIDE) {
                                 if (CV_MAT_ELEM(*f,uchar,k-1,l)!=INSIDE) {
                                    // gradI.y=(float)((CV_MAT_ELEM(*out,uchar,kp+1,lm)-CV_MAT_ELEM(*out,uchar,km-1,lm)))*2.0f;
                                    gradI.y=(float)((CV_MAT_ELEM(*out,uint16_t,kp+1,lm)-CV_MAT_ELEM(*out,uint16_t,km-1,lm)))*2.0f;
                                 } else {
                                    // gradI.y=(float)((CV_MAT_ELEM(*out,uchar,kp+1,lm)-CV_MAT_ELEM(*out,uchar,km,lm)));
                                    gradI.y=(float)((CV_MAT_ELEM(*out,uint16_t,kp+1,lm)-CV_MAT_ELEM(*out,uint16_t,km,lm)));
                                 }
                              } else {
                                 if (CV_MAT_ELEM(*f,uchar,k-1,l)!=INSIDE) {
                                    // gradI.y=(float)((CV_MAT_ELEM(*out,uchar,kp,lm)-CV_MAT_ELEM(*out,uchar,km-1,lm)));
                                    gradI.y=(float)((CV_MAT_ELEM(*out,uint16_t,kp,lm)-CV_MAT_ELEM(*out,uint16_t,km-1,lm)));
                                 } else {
                                    gradI.y=0;
                                 }
                              }
                              // Ia += (float)w * (float)(CV_MAT_ELEM(*out,uchar,km,lm));
                              Ia += (float)w * (float)(CV_MAT_ELEM(*out,uint16_t,km,lm));
                              Jx -= (float)w * (float)(gradI.x*r.x);
                              Jy -= (float)w * (float)(gradI.y*r.y);
                              s  += w;
                           }
                        }
                     }
                  }
                  sat = (float)((Ia/s+(Jx+Jy)/(sqrt(Jx*Jx+Jy*Jy)+1.0e-20f)+0.5f));
                  {
                  // CV_MAT_ELEM(*out,uchar,i-1,j-1) = cv::saturate_cast<uchar>(sat);
                  CV_MAT_ELEM(*out,uint16_t,i-1,j-1) = cv::saturate_cast<uint16_t>(sat);
                  }
               }

               CV_MAT_ELEM(*f,uchar,i,j) = BAND;
               Heap->Push(i,j,dist);
            }
         }
      }
   }
}


static void
icvNSInpaintFMM(const CvMat *f, CvMat *t, CvMat *out, int range, CvPriorityQueueFloat *Heap) {
   int i = 0, j = 0, ii = 0, jj = 0, k, l, q;
   float dist;

   if (CV_MAT_CN(out->type)==1) {

      while (Heap->Pop(&ii,&jj)) {

         CV_MAT_ELEM(*f,uchar,ii,jj) = KNOWN;
         for(q=0; q<4; q++) {
            if     (q==0) {i=ii-1; j=jj;}
            else if(q==1) {i=ii;   j=jj-1;}
            else if(q==2) {i=ii+1; j=jj;}
            else if(q==3) {i=ii;   j=jj+1;}
            if ((i<=1)||(j<=1)||(i>t->rows-1)||(j>t->cols-1)) continue;

            if (CV_MAT_ELEM(*f,uchar,i,j)==INSIDE) {
               dist = min4(FastMarching_solve(i-1,j,i,j-1,f,t),
                           FastMarching_solve(i+1,j,i,j-1,f,t),
                           FastMarching_solve(i-1,j,i,j+1,f,t),
                           FastMarching_solve(i+1,j,i,j+1,f,t));
               CV_MAT_ELEM(*t,float,i,j) = dist;

               {
                  CvPoint2D32f gradI,r;
                  float Ia=0,s=1.0e-20f,w,dst,dir;

                  for (k=i-range; k<=i+range; k++) {
                     int km=k-1+(k==1),kp=k-1-(k==t->rows-2);
                     for (l=j-range; l<=j+range; l++) {
                        int lm=l-1+(l==1),lp=l-1-(l==t->cols-2);
                        if (k>0&&l>0&&k<t->rows-1&&l<t->cols-1) {
                           if ((CV_MAT_ELEM(*f,uchar,k,l)!=INSIDE)&&
                               ((l-j)*(l-j)+(k-i)*(k-i)<=range*range)) {
                              r.y=(float)(i-k);
                              r.x=(float)(j-l);

                              dst = 1/(VectorLength(r)*VectorLength(r)+1);

                              if (CV_MAT_ELEM(*f,uchar,k+1,l)!=INSIDE) {
                                 if (CV_MAT_ELEM(*f,uchar,k-1,l)!=INSIDE) {
                                    // gradI.x=(float)(abs(CV_MAT_ELEM(*out,uchar,kp+1,lm)-CV_MAT_ELEM(*out,uchar,kp,lm))+
                                                    // abs(CV_MAT_ELEM(*out,uchar,kp,lm)-CV_MAT_ELEM(*out,uchar,km-1,lm)));
                                    gradI.x=(float)(abs(CV_MAT_ELEM(*out,uint16_t,kp+1,lm)-CV_MAT_ELEM(*out,uint16_t,kp,lm))+
                                                    abs(CV_MAT_ELEM(*out,uint16_t,kp,lm)-CV_MAT_ELEM(*out,uint16_t,km-1,lm)));
                                 } else {
                                    // gradI.x=(float)(abs(CV_MAT_ELEM(*out,uchar,kp+1,lm)-CV_MAT_ELEM(*out,uchar,kp,lm)))*2.0f;
                                    gradI.x=(float)(abs(CV_MAT_ELEM(*out,uint16_t,kp+1,lm)-CV_MAT_ELEM(*out,uint16_t,kp,lm)))*2.0f;
                                 }
                              } else {
                                 if (CV_MAT_ELEM(*f,uchar,k-1,l)!=INSIDE) {
                                    // gradI.x=(float)(abs(CV_MAT_ELEM(*out,uchar,kp,lm)-CV_MAT_ELEM(*out,uchar,km-1,lm)))*2.0f;
                                    gradI.x=(float)(abs(CV_MAT_ELEM(*out,uint16_t,kp,lm)-CV_MAT_ELEM(*out,uint16_t,km-1,lm)))*2.0f;
                                 } else {
                                    gradI.x=0;
                                 }
                              }
                              if (CV_MAT_ELEM(*f,uchar,k,l+1)!=INSIDE) {
                                 if (CV_MAT_ELEM(*f,uchar,k,l-1)!=INSIDE) {
                                    // gradI.y=(float)(abs(CV_MAT_ELEM(*out,uchar,km,lp+1)-CV_MAT_ELEM(*out,uchar,km,lm))+
                                                    // abs(CV_MAT_ELEM(*out,uchar,km,lm)-CV_MAT_ELEM(*out,uchar,km,lm-1)));
                                    gradI.y=(float)(abs(CV_MAT_ELEM(*out,uint16_t,km,lp+1)-CV_MAT_ELEM(*out,uint16_t,km,lm))+
                                                    abs(CV_MAT_ELEM(*out,uint16_t,km,lm)-CV_MAT_ELEM(*out,uint16_t,km,lm-1)));
                                 } else {
                                    // gradI.y=(float)(abs(CV_MAT_ELEM(*out,uchar,km,lp+1)-CV_MAT_ELEM(*out,uchar,km,lm)))*2.0f;
                                    gradI.y=(float)(abs(CV_MAT_ELEM(*out,uint16_t,km,lp+1)-CV_MAT_ELEM(*out,uint16_t,km,lm)))*2.0f;
                                 }
                              } else {
                                 if (CV_MAT_ELEM(*f,uchar,k,l-1)!=INSIDE) {
                                    // gradI.y=(float)(abs(CV_MAT_ELEM(*out,uchar,km,lm)-CV_MAT_ELEM(*out,uchar,km,lm-1)))*2.0f;
                                    gradI.y=(float)(abs(CV_MAT_ELEM(*out,uint16_t,km,lm)-CV_MAT_ELEM(*out,uint16_t,km,lm-1)))*2.0f;
                                 } else {
                                    gradI.y=0;
                                 }
                              }

                              gradI.x=-gradI.x;
                              dir=VectorScalMult(r,gradI);

                              if (fabs(dir)<=0.01) {
                                 dir=0.000001f;
                              } else {
                                 dir = (float)fabs(VectorScalMult(r,gradI)/sqrt(VectorLength(r)*VectorLength(gradI)));
                              }
                              w = dst*dir;
                              // Ia += (float)w * (float)(CV_MAT_ELEM(*out,uchar,km,lm));
                              Ia += (float)w * (float)(CV_MAT_ELEM(*out,uint16_t,km,lm));
                              s  += w;
                           }
                        }
                     }
                  }
                  // CV_MAT_ELEM(*out,uchar,i-1,j-1) = cv::saturate_cast<uchar>((double)Ia/s);
                  CV_MAT_ELEM(*out,uint16_t,i-1,j-1) = cv::saturate_cast<uint16_t>((double)Ia/s);
               }

               CV_MAT_ELEM(*f,uchar,i,j) = BAND;
               Heap->Push(i,j,dist);
            }
         }
      }

   }
}

#define SET_BORDER1_C1(image,type,value) {\
      int i,j;\
      for(j=0; j<image->cols; j++) {\
         CV_MAT_ELEM(*image,type,0,j) = value;\
      }\
      for (i=1; i<image->rows-1; i++) {\
         CV_MAT_ELEM(*image,type,i,0) = CV_MAT_ELEM(*image,type,i,image->cols-1) = value;\
      }\
      for(j=0; j<image->cols; j++) {\
         CV_MAT_ELEM(*image,type,erows-1,j) = value;\
      }\
   }

#define COPY_MASK_BORDER1_C1(src,dst,type) {\
      int i,j;\
      for (i=0; i<src->rows; i++) {\
         for(j=0; j<src->cols; j++) {\
            if (CV_MAT_ELEM(*src,type,i,j)!=0)\
               CV_MAT_ELEM(*dst,type,i+1,j+1) = INSIDE;\
         }\
      }\
   }


void
_cvInpaint( const CvArr* _input_img, const CvArr* _inpaint_mask, CvArr* _output_img,
           double inpaintRange, int flags )
{
    cv::Ptr<CvMat> mask, band, f, t, out;
    cv::Ptr<CvPriorityQueueFloat> Heap, Out;
    IplConvKernel *el_cross, *el_range;

    CvMat input_hdr, mask_hdr, output_hdr;
    CvMat* input_img, *inpaint_mask, *output_img;
    int range=cvRound(inpaintRange);
    int erows, ecols;

    input_img = cvGetMat( _input_img, &input_hdr );
    inpaint_mask = cvGetMat( _inpaint_mask, &mask_hdr );
    output_img = cvGetMat( _output_img, &output_hdr );

    if( !CV_ARE_SIZES_EQ(input_img,output_img) || !CV_ARE_SIZES_EQ(input_img,inpaint_mask))
        CV_Error( CV_StsUnmatchedSizes, "All the input and output images must have the same size" );

    if( (CV_MAT_TYPE(input_img->type) != CV_16UC1) ||
        !CV_ARE_TYPES_EQ(input_img,output_img) )
        CV_Error( CV_StsUnsupportedFormat,
        "Only 8-bit 1-channel and 3-channel input/output images are supported" );

    if( CV_MAT_TYPE(inpaint_mask->type) != CV_8UC1 )
        CV_Error( CV_StsUnsupportedFormat, "The mask must be 8-bit 1-channel image" );

    range = MAX(range,1);
    range = MIN(range,100);

    ecols = input_img->cols + 2;
    erows = input_img->rows + 2;

    f = cvCreateMat(erows, ecols, CV_8UC1);
    t = cvCreateMat(erows, ecols, CV_32FC1);
    band = cvCreateMat(erows, ecols, CV_8UC1);
    mask = cvCreateMat(erows, ecols, CV_8UC1);
    el_cross = cvCreateStructuringElementEx(3,3,1,1,CV_SHAPE_CROSS,NULL);

    cvCopy( input_img, output_img );
    cvSet(mask,cvScalar(KNOWN,0,0,0));
    COPY_MASK_BORDER1_C1(inpaint_mask,mask,uchar);
    SET_BORDER1_C1(mask,uchar,0);
    cvSet(f,cvScalar(KNOWN,0,0,0));
    cvSet(t,cvScalar(1.0e6f,0,0,0));
    cvDilate(mask,band,el_cross,1);   // image with narrow band
    cvReleaseStructuringElement(&el_cross);
    Heap=new CvPriorityQueueFloat;
    if (!Heap->Init(band))
        return;
    cvSub(band,mask,band,NULL);
    SET_BORDER1_C1(band,uchar,0);
    if (!Heap->Add(band))
        return;
    cvSet(f,cvScalar(BAND,0,0,0),band);
    cvSet(f,cvScalar(INSIDE,0,0,0),mask);
    cvSet(t,cvScalar(0,0,0,0),band);

    if( flags == CV_INPAINT_TELEA )
    {
        out = cvCreateMat(erows, ecols, CV_8UC1);
        el_range = cvCreateStructuringElementEx(2*range+1,2*range+1,
            range,range,CV_SHAPE_RECT,NULL);
        cvDilate(mask,out,el_range,1);
        cvReleaseStructuringElement(&el_range);
        cvSub(out,mask,out,NULL);
        Out=new CvPriorityQueueFloat;
        if (!Out->Init(out))
            return;
        if (!Out->Add(band))
            return;
        cvSub(out,band,out,NULL);
        SET_BORDER1_C1(out,uchar,0);
        icvCalcFMM(out,t,Out,true);
        icvTeleaInpaintFMM(mask,t,output_img,range,Heap);
    }
    else if (flags == CV_INPAINT_NS) {
        icvNSInpaintFMM(mask,t,output_img,range,Heap);
    } else {
        CV_Error( CV_StsBadArg, "The flags argument must be one of CV_INPAINT_TELEA or CV_INPAINT_NS" );
    }
}

CvMat ToCvMat(const cv::Mat& m)
{
	CV_DbgAssert(m.dims <= 2);
	CvMat dst = cvMat(m.rows, m.dims == 1 ? 1 : m.cols, m.type(), m.data);
	dst.step = (int)m.step[0];
	dst.type = (dst.type & ~cv::Mat::CONTINUOUS_FLAG) | (m.flags & cv::Mat::CONTINUOUS_FLAG);
	return dst;
}

void _inpaint( InputArray _src, InputArray _mask, OutputArray _dst,
                  double inpaintRange, int flags )
{
    Mat src = _src.getMat(), mask = _mask.getMat();
    _dst.create( src.size(), src.type() );
    CvMat c_src = ToCvMat(src), c_mask = ToCvMat(mask), c_dst = ToCvMat(_dst.getMat());
    _cvInpaint( &c_src, &c_mask, &c_dst, inpaintRange, flags );
}

//////////////////////////////////////////////////////////////////////////////////////

cv::Mat DepthInpainter::genValidMask(const cv::Mat& depth)
{
    cv::Mat orgMask = (depth == 0);
    // cv::Mat mask = orgMask.clone();
    cv::Mat mask = orgMask;

    cv::Mat kernel = cv::Mat::zeros(_kernelSize, _kernelSize, CV_8U);
    cv::circle(kernel, cv::Point(kernel.cols/2, kernel.rows/2), kernel.rows/2, cv::Scalar(255), -1);
    cv::erode(orgMask, mask, kernel);
    cv::dilate(mask, mask, kernel);

    gSpeckleFilter.Compute(mask, 0, _maxInternalHoleToBeFilled, 1);

    // revert mask
    mask = mask == 0;

    return mask;
}

void DepthInpainter::inpaint(const cv::Mat& depth, cv::Mat& out, const cv::Mat& mask)
{
    cv::Mat newDepth;
    cv::Mat _mask = mask.empty() ? (depth == 0) : mask;
    if(depth.type() == CV_8U || depth.type() == CV_8UC3){
        cv::inpaint(depth, _mask, newDepth, _inpaintRadius, cv::INPAINT_TELEA);
    } else if(depth.type() == CV_16U){
        _inpaint(depth, _mask, newDepth, _inpaintRadius, cv::INPAINT_TELEA);
    }

    if(mask.empty() && !_fillAll){
        // gen masked image
        cv::Mat mask = genValidMask(depth);
        out = cv::Mat::zeros(depth.size(), CV_16U);
        newDepth.copyTo(out, mask);
    } else {
        out = newDepth;
    }
}
