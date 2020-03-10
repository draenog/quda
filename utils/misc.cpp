#include <stdio.h>
#include <stdlib.h>
#include "quda.h"
#include <string.h>
#include "invert_quda.h"
#include "misc.h"
#include <assert.h>
#include "util_quda.h"
#include <host_utils.h>


/*
#define stSpinorSiteSize 6
template<typename Float>
void display_spinor_internal(Float* spinor)
{
    printf("(%f,%f) (%f,%f) (%f,%f) \t", 
	   spinor[0], spinor[1], spinor[2], 
	   spinor[3], spinor[4], spinor[5]);
    
    printf("\n");
    return;
}



void display_spinor(void* spinor, int len, int precision)
{    
    int i;
    
    if (precision == QUDA_DOUBLE_PRECISION){
	double* myspinor = (double*)spinor;
	for (i = 0;i < len; i++){
	    display_spinor_internal(myspinor + stSpinorSiteSize*i);
	}
    }else if (precision == QUDA_SINGLE_PRECISION){
	float* myspinor = (float*)spinor;
	for (i=0;i < len ;i++){
	    display_spinor_internal(myspinor + stSpinorSiteSize*i);
	}
    }
    return;
}



template<typename Float>
void display_link_internal(Float* link)
{
    int i, j;
    
    for (i = 0;i < 3; i++){
	for(j=0;j < 3; j++){
	    printf("(%.10f,%.10f) \t", link[i*3*2 + j*2], link[i*3*2 + j*2 + 1]);
	}
	printf("\n");
    }
    printf("\n");
    return;
}



void display_link(void* link, int len, int precision)
{    
    int i;
    
    if (precision == QUDA_DOUBLE_PRECISION){
	double* mylink = (double*)link;
	for (i = 0;i < len; i++){
	    display_link_internal(mylink + gauge_site_size*i);
	}
    }else if (precision == QUDA_SINGLE_PRECISION){
	float* mylink = (float*)link;
	for (i=0;i < len ;i++){
	    display_link_internal(mylink + gauge_site_size*i);
	}
    }
    return;
}



template <typename Float>
void accumulateConjugateProduct(Float *a, Float *b, Float *c, int sign) {
  a[0] += sign * (b[0]*c[0] - b[1]*c[1]);
  a[1] -= sign * (b[0]*c[1] + b[1]*c[0]);
}


template<typename Float>
int link_sanity_check_internal_12(Float* link, int dir, int ga_idx, QudaGaugeParam* gaugeParam, int oddBit)
{
    //printf("link sanity check is called\n");
    
    int ret =0;
    
    Float refc_buf[6];
    Float* refc = &refc_buf[0];

    memset((void*)refc, 0, sizeof(refc_buf));

    Float* a = link;
    Float* b = link + 6;
    Float* c = link + 12;
    
    accumulateConjugateProduct(refc + 0*2, a + 1*2, b + 2*2, +1);
    accumulateConjugateProduct(refc + 0*2, a + 2*2, b + 1*2, -1);
    accumulateConjugateProduct(refc + 1*2, a + 2*2, b + 0*2, +1);
    accumulateConjugateProduct(refc + 1*2, a + 0*2, b + 2*2, -1);
    accumulateConjugateProduct(refc + 2*2, a + 0*2, b + 1*2, +1);
    accumulateConjugateProduct(refc + 2*2, a + 1*2, b + 0*2, -1);
    
    int X1h=gaugeParam->X[0]/2;
    int X1 =gaugeParam->X[0];    
    int X2 =gaugeParam->X[1];
    int X3 =gaugeParam->X[2];
    int X4 =gaugeParam->X[3];
    double t_boundary = (gaugeParam->t_boundary ==QUDA_ANTI_PERIODIC_T)? -1.0:1.0;

   double u0 = gaugeParam->tadpole_coeff;
   double coff= -u0*u0*24;
   //coff = (dir < 6) ? coff : ( (ga_idx >= (X4-3)*X1h*X2*X3 )? t_boundary : 1); 

   //float u0 = (dir < 6) ? gaugeParam->anisotropy : ( (ga_idx >= (X4-3)*X1h*X2*X3 )? t_boundary : 1); 

  
#if 1
   
   {
       int index = fullLatticeIndex(ga_idx, oddBit);
       int i4 = index /(X3*X2*X1);
       int i3 = (index - i4*(X3*X2*X1))/(X2*X1);
       int i2 = (index - i4*(X3*X2*X1) - i3*(X2*X1))/X1;
       int i1 = index - i4*(X3*X2*X1) - i3*(X2*X1) - i2*X1;
       
       if (dir == 0) {
           if (i4 % 2 == 1){
               coff *= -1;
           }
       }

       if (dir == 2){
           if ((i1+i4) % 2 == 1){
               coff *= -1;
           }
       }
       if (dir == 4){
           if ( (i4+i1+i2) % 2 == 1){
               coff *= -1;
           }
       }
       if (dir == 6){
           if (ga_idx >= (X4-3)*X1h*X2*X3 ){
               coff *= -1;
           } 
       }

       //printf("local ga_idx =%d, index=%d, i4,3,2,1 =%d %d %d %d\n", ga_idx, index, i4, i3, i2,i1);
 
   }
#endif
 

   refc[0]*=coff; refc[1]*=coff; refc[2]*=coff; refc[3]*=coff; refc[4]*=coff; refc[5]*=coff;
   
    
    double delta = 0.0001;
    int i;
    for (i =0;i < 6; i++){
	double diff =  refc[i] -  c[i];
	double absdiff = diff > 0? diff: (-diff);
	if (absdiff  > delta){
	    printf("ERROR: sanity check failed for link\n");
	    display_link_internal(link);
	    printf("refc = (%.10f,%.10f) (%.10f,%.10f) (%.10f,%.10f)\n", 
		   refc[0], refc[1], refc[2], refc[3], refc[4], refc[5]);
	    printf("dir=%d, ga_idx=%d, coff=%f, t_boundary=%f\n",dir, ga_idx,coff, t_boundary);
	    printf("X=%d %d %d %d, X1h=%d\n", gaugeParam->X[0], X2, X3, X4, X1h);
	    return -1;
	}
	
    }
    

    return ret;
}


template<typename Float>
int site_link_sanity_check_internal_12(Float* link, int dir, int ga_idx, QudaGaugeParam* gaugeParam, int oddBit)
{
    int ret = 0;
    
    Float refc_buf[6];
    Float* refc = &refc_buf[0];

    memset((void*)refc, 0, sizeof(refc_buf));

    Float* a = link;
    Float* b = link + 6;
    Float* c = link + 12;
    
    accumulateConjugateProduct(refc + 0*2, a + 1*2, b + 2*2, +1);
    accumulateConjugateProduct(refc + 0*2, a + 2*2, b + 1*2, -1);
    accumulateConjugateProduct(refc + 1*2, a + 2*2, b + 0*2, +1);
    accumulateConjugateProduct(refc + 1*2, a + 0*2, b + 2*2, -1);
    accumulateConjugateProduct(refc + 2*2, a + 0*2, b + 1*2, +1);
    accumulateConjugateProduct(refc + 2*2, a + 1*2, b + 0*2, -1);

    int X1h=gaugeParam->X[0]/2;
    int X1 =gaugeParam->X[0];    
    int X2 =gaugeParam->X[1];
    int X3 =gaugeParam->X[2];
    int X4 =gaugeParam->X[3];

#if 1        
    double coeff= 1.0;
   
   {
       int index = fullLatticeIndex(ga_idx, oddBit);
       int i4 = index /(X3*X2*X1);
       int i3 = (index - i4*(X3*X2*X1))/(X2*X1);
       int i2 = (index - i4*(X3*X2*X1) - i3*(X2*X1))/X1;
       int i1 = index - i4*(X3*X2*X1) - i3*(X2*X1) - i2*X1;
       
       if (dir == XUP) {
           if (i4 % 2 == 1){
               coeff *= -1;
           }
       }

       if (dir == YUP){
           if ((i1+i4) % 2 == 1){
               coeff *= -1;
           }
       }
       if (dir == ZUP){
           if ( (i4+i1+i2) % 2 == 1){
               coeff *= -1;
           }
       }
       if (dir == TUP){
         if (last_node_in_t() && i4 == (X4 - 1)) { coeff *= -1; }
       }       
   }
 
   
   refc[0]*=coeff; refc[1]*=coeff; refc[2]*=coeff; refc[3]*=coeff; refc[4]*=coeff; refc[5]*=coeff;
#endif
   
    
    double delta = 0.0001;
    int i;
    for (i =0;i < 6; i++){
	double diff =  refc[i] -  c[i];
	double absdiff = diff > 0? diff: (-diff);
	if (absdiff  > delta){
	    printf("ERROR: sanity check failed for site link\n");
	    display_link_internal(link);
	    printf("refc = (%.10f,%.10f) (%.10f,%.10f) (%.10f,%.10f)\n", 
		   refc[0], refc[1], refc[2], refc[3], refc[4], refc[5]);
	    printf("X=%d %d %d %d, X1h=%d\n", gaugeParam->X[0], X2, X3, X4, X1h);
	    return -1;
	}
	
    }
    

    return ret;
}






// a+=b
template <typename Float>
void complexAddTo(Float *a, Float *b) {
  a[0] += b[0];
  a[1] += b[1];
}

// a = b*c
template <typename Float>
void complexProduct(Float *a, Float *b, Float *c) {
    a[0] = b[0]*c[0] - b[1]*c[1];
    a[1] = b[0]*c[1] + b[1]*c[0];
}

// a = conj(b)*conj(c)
template <typename Float>
void complexConjugateProduct(Float *a, Float *b, Float *c) {
    a[0] = b[0]*c[0] - b[1]*c[1];
    a[1] = -b[0]*c[1] - b[1]*c[0];
}

// a = conj(b)*c
template <typename Float>
void complexDotProduct(Float *a, Float *b, Float *c) {
    a[0] = b[0]*c[0] + b[1]*c[1];
    a[1] = b[0]*c[1] - b[1]*c[0];
}

// a += b*c
template <typename Float>
void accumulateComplexProduct(Float *a, Float *b, Float *c, Float sign) {
  a[0] += sign*(b[0]*c[0] - b[1]*c[1]);
  a[1] += sign*(b[0]*c[1] + b[1]*c[0]);
}

// a += conj(b)*c)
template <typename Float>
void accumulateComplexDotProduct(Float *a, Float *b, Float *c) {
    a[0] += b[0]*c[0] + b[1]*c[1];
    a[1] += b[0]*c[1] - b[1]*c[0];
}


template<typename Float>
int link_sanity_check_internal_8(Float* link, int dir, int ga_idx, QudaGaugeParam* gaugeParam, int oddBit) 
{
    int ret =0;
    
    Float ref_link_buf[18];
    Float* ref = & ref_link_buf[0];    
    memset(ref, 0, sizeof(ref_link_buf));
    
    ref[0] = atan2(link[1], link[0]);
    ref[1] = atan2(link[13], link[12]);
    for (int i=2; i<7; i++) {
	ref[i] = link[i];
    }

    int X1h=gaugeParam->X[0]/2;
    int X2 =gaugeParam->X[1];
    int X3 =gaugeParam->X[2];
    int X4 =gaugeParam->X[3];
    double t_boundary = (gaugeParam->t_boundary ==QUDA_ANTI_PERIODIC_T)? -1.0:1.0;
    
    
    // First reconstruct first row
    Float row_sum = 0.0;
    row_sum += ref[2]*ref[2];
    row_sum += ref[3]*ref[3];
    row_sum += ref[4]*ref[4];
    row_sum += ref[5]*ref[5];

#define SMALL_NUM 1e-24
    row_sum = (row_sum != 0)?row_sum: SMALL_NUM;
#if 1
    Float u0= -gaugeParam->tadpole_coeff*gaugeParam->tadpole_coeff*24;
    {
	int X1h=gaugeParam->X[0]/2;
	int X1 =gaugeParam->X[0];
	int X2 =gaugeParam->X[1];
	int X3 =gaugeParam->X[2];
	int X4 =gaugeParam->X[3];
      
	int index = fullLatticeIndex(ga_idx, oddBit);
	int i4 = index /(X3*X2*X1);
	int i3 = (index - i4*(X3*X2*X1))/(X2*X1);
	int i2 = (index - i4*(X3*X2*X1) - i3*(X2*X1))/X1;
	int i1 = index - i4*(X3*X2*X1) - i3*(X2*X1) - i2*X1;
      
	if (dir == 0) {
	    if (i4 % 2 == 1){
		u0 *= -1;
	    }
	}
      
	if (dir == 1){
	    if ((i1+i4) % 2 == 1){
		u0 *= -1;
	    }
	}
	if (dir == 2){
	    if ( (i4+i1+i2) % 2 == 1){
		u0 *= -1;
	    }
	}
	if (dir == 3){
	    if (ga_idx >= (X4-3)*X1h*X2*X3 ){
		u0 *= -1;
	    }
	}
       
	//printf("local ga_idx =%d, index=%d, i4,3,2,1 =%d %d %d %d\n", ga_idx, index, i4, i3, i2,i1);
       
    }
#endif
    

    Float U00_mag = sqrt( (1.f/(u0*u0) - row_sum)>0? (1.f/(u0*u0)-row_sum):0);
  
    ref[14] = ref[0];
    ref[15] = ref[1];

    ref[0] = U00_mag * cos(ref[14]);
    ref[1] = U00_mag * sin(ref[14]);

    Float column_sum = 0.0;
    for (int i=0; i<2; i++) column_sum += ref[i]*ref[i];
    for (int i=6; i<8; i++) column_sum += ref[i]*ref[i];
    Float U20_mag = sqrt( (1.f/(u0*u0) - column_sum) > 0? (1.f/(u0*u0)-column_sum) : 0);

    ref[12] = U20_mag * cos(ref[15]);
    ref[13] = U20_mag * sin(ref[15]);

    // First column now restored

    // finally reconstruct last elements from SU(2) rotation
    Float r_inv2 = 1.0/(u0*row_sum);

    // U11
    Float A[2];
    complexDotProduct(A, ref+0, ref+6);
    complexConjugateProduct(ref+8, ref+12, ref+4);
    accumulateComplexProduct(ref+8, A, ref+2, u0);
    ref[8] *= -r_inv2;
    ref[9] *= -r_inv2;

    // U12
    complexConjugateProduct(ref+10, ref+12, ref+2);
    accumulateComplexProduct(ref+10, A, ref+4, -u0);
    ref[10] *= r_inv2;
    ref[11] *= r_inv2;

    // U21
    complexDotProduct(A, ref+0, ref+12);
    complexConjugateProduct(ref+14, ref+6, ref+4);
    accumulateComplexProduct(ref+14, A, ref+2, -u0);
    ref[14] *= r_inv2;
    ref[15] *= r_inv2;

    // U12
    complexConjugateProduct(ref+16, ref+6, ref+2);
    accumulateComplexProduct(ref+16, A, ref+4, u0);
    ref[16] *= -r_inv2;
    ref[17] *= -r_inv2;

    double delta = 0.0001;
    int i;
    for (i =0;i < 18; i++){

	double diff =  ref[i] -  link[i];
	double absdiff = diff > 0? diff: (-diff);
	if ( (ref[i] !=  ref[i]) || (absdiff  > delta)){
	    printf("ERROR: sanity check failed for link\n");
	    display_link_internal(link);
	    printf("reconstructed link is\n");
	    display_link_internal(ref);
	    printf("dir=%d, ga_idx=%d, u0=%f, t_boundary=%f\n",dir, ga_idx, u0, t_boundary);
	    printf("X=%d %d %d %d, X1h=%d\n", gaugeParam->X[0], X2, X3, X4, X1h);
	    return -1;
	}
	
    }
    

    return ret;
}


//this len must be V
int
link_sanity_check(void* link, int len, int precision, int dir, QudaGaugeParam* gaugeParam)
{
    int i;
    int rc = 0;
    
    if (precision == QUDA_DOUBLE_PRECISION){
	double* mylink = (double*)link;
	//even
	for (i = 0;i < len/2; i++){
	    rc = link_sanity_check_internal_12(mylink + gauge_site_size*i, dir, i, gaugeParam, 0);
	    if (rc != 0){
		printf("ERROR: even link sanity check failed, i=%d\n",i);
		display_link_internal(mylink+gauge_site_size*i);
		exit(1);
	    }
	}
	
	mylink = mylink + gauge_site_size*len/2;
	//odd
	for (i = 0;i < len/2; i++){
	    rc = link_sanity_check_internal_12(mylink + gauge_site_size*i, dir, i, gaugeParam, 1);
	    if (rc != 0){
		printf("ERROR: odd link sanity check failed, i=%d\n",i);
		display_link_internal(mylink+gauge_site_size*i);
		exit(1);
	    }
	}	
	
    }else if (precision == QUDA_SINGLE_PRECISION){
	float* mylink = (float*)link;

	//even
	for (i=0;i < len/2 ;i++){
	    rc = link_sanity_check_internal_12(mylink + gauge_site_size*i, dir, i, gaugeParam, 0);
	    if (rc != 0){
		printf("ERROR: even link sanity check 12 failed, i=%d\n",i);
		exit(1);
	    }
	    
	    // rc = link_sanity_check_internal_8(mylink + gauge_site_size*i, dir, i, gaugeParam, 0);
	    // if (rc != 0){
	    // 	printf("ERROR: even link sanity check 8 failed, i=%d\n",i);
	    // 	exit(1);
	    // }
	    
	    
	}
	mylink = mylink + gauge_site_size*len/2;
	//odd
	for (i=0;i < len/2 ;i++){
	    rc = link_sanity_check_internal_12(mylink + gauge_site_size*i, dir, i, gaugeParam, 1);
	    if (rc != 0){
		printf("ERROR: odd link sanity check 12 failed, i=%d\n", i);
		exit(1);
	    }	
	    // rc = link_sanity_check_internal_8(mylink + gauge_site_size*i, dir, i, gaugeParam, 0);
	    // if (rc != 0){
	    // 	printf("ERROR: even link sanity check 8 failed, i=%d\n",i);
	    // 	exit(1);
	    // }
	}	

    }
    
    return rc;
}



//this len must be V
int
site_link_sanity_check(void* link, int len, int precision, QudaGaugeParam* gaugeParam)
{
    int i;
    int rc = 0;
    int dir;
    
    if (precision == QUDA_DOUBLE_PRECISION){
	double* mylink = (double*)link;
	//even	
	for (i = 0;i < len/2; i++){
	    for(dir=XUP;dir <= TUP; dir++){
		rc = site_link_sanity_check_internal_12(mylink + gauge_site_size*(4*i+dir), dir, i, gaugeParam, 0);
		if (rc != 0){
		    printf("ERROR: even link sanity check failed, i=%d, function %s\n",i, __FUNCTION__);
		    display_link_internal(mylink+gauge_site_size*i);
		    exit(1);
		}
	    }
	}
	
	mylink = mylink + 4*gauge_site_size*len/2;
	//odd
	for (i = 0;i < len/2; i++){
	    for(dir=XUP;dir <= TUP; dir++){	    
		rc = site_link_sanity_check_internal_12(mylink + gauge_site_size*(4*i+dir), dir, i, gaugeParam, 1);
		if (rc != 0){
		    printf("ERROR: odd link sanity check failed, i=%d, function %s\n",i, __FUNCTION__);
		    display_link_internal(mylink+gauge_site_size*i);
		    exit(1);
		}
	    }
	}	
	
    }else if (precision == QUDA_SINGLE_PRECISION){
	float* mylink = (float*)link;

	//even
	for (i=0;i < len/2 ;i++){
	    for(dir=XUP;dir <= TUP; dir++){
		rc = site_link_sanity_check_internal_12(mylink + gauge_site_size*(4*i+dir), dir, i, gaugeParam, 0);
		if (rc != 0){
		    printf("ERROR: even link sanity check 12 failed, i=%d, function %s\n",i, __FUNCTION__);
		    exit(1);
		}
	    }
	}
	mylink = mylink + 4*gauge_site_size*len/2;
	//odd
	for (i=0;i < len/2 ;i++){
	    for(dir=XUP;dir <= TUP; dir++){
		rc = site_link_sanity_check_internal_12(mylink + gauge_site_size*(4*i+dir), dir, i, gaugeParam, 1);
		if (rc != 0){
		    printf("ERROR: odd link sanity check 12 failed, i=%d, function %s\n", i, __FUNCTION__);
		    exit(1);
		}	
	    }
	}	

    }
    
    return rc;
}
*/

const char *
get_verbosity_str(QudaVerbosity type)
{
  const char* ret;

  switch(type) {
  case QUDA_SILENT:
    ret = "silent";
    break;
  case QUDA_SUMMARIZE:
    ret = "summarize";
    break;
  case QUDA_VERBOSE:
    ret = "verbose";
    break;
  case QUDA_DEBUG_VERBOSE:
    ret = "debug";
    break;
  default:
    fprintf(stderr, "Error: invalid verbosity type %d\n", type);
    exit(1);
  }

  return ret;
}

const char*
get_prec_str(QudaPrecision prec)
{
  const char* ret;

  switch (prec) {
  case QUDA_DOUBLE_PRECISION:
    ret=  "double";
    break;
  case QUDA_SINGLE_PRECISION:
    ret= "single";
    break;
  case QUDA_HALF_PRECISION:
    ret= "half";
    break;
  case QUDA_QUARTER_PRECISION:
    ret= "quarter";
    break;
  default:
    ret = "unknown";
    break;
  }

  return ret;
}

const char* 
get_unitarization_str(bool svd_only)
{
  const char* ret;
 
  if(svd_only){
    ret = "SVD";
  }else{
    ret = "Cayley-Hamilton/SVD";
  }
  return ret;
}

const char* 
get_gauge_order_str(QudaGaugeFieldOrder order)
{
  const char* ret;

  switch(order){
    case QUDA_QDP_GAUGE_ORDER:
	ret = "qdp";
	break;

    case QUDA_MILC_GAUGE_ORDER:
	ret = "milc";
	break;

    case QUDA_CPS_WILSON_GAUGE_ORDER:
	ret = "cps_wilson";
	break;

    default:
	ret = "unknown";
	break;
  }	

  return ret;
}


const char* 
get_recon_str(QudaReconstructType recon)
{
    const char* ret;
    switch(recon){
    case QUDA_RECONSTRUCT_13:
        ret="13";
        break;
    case QUDA_RECONSTRUCT_12:
	ret= "12";
	break;
    case QUDA_RECONSTRUCT_9:
        ret="9";
        break;
    case QUDA_RECONSTRUCT_8:
	ret = "8";
	break;
    case QUDA_RECONSTRUCT_NO:
	ret = "18";
	break;
    default:
	ret="unknown";
	break;
    }
    
    return ret;
}

const char*
get_test_type(int t)
{
    const char* ret;
    switch(t){
    case 0:
	ret = "even";
	break;
    case 1:
	ret = "odd";
	break;
    case 2:
	ret = "full";
	break;
    case 3:
	ret = "mcg_even";
	break;	
    case 4:
	ret = "mcg_odd";
	break;	
    case 5:
	ret = "mcg_full";
	break;	
    default:
	ret = "unknown";
	break;
    }
    
    return ret;
}

const char*
get_staggered_test_type(int t)
{
    const char* ret;
    switch(t){
    case 0:
  ret = "full";
  break;
    case 1:
  ret = "full_ee_prec";
  break;
    case 2:
  ret = "full_oo_prec";
  break;
    case 3:
  ret = "even";
  break;
    case 4:
  ret = "odd";
  break;
    case 5:
  ret = "mcg_even";
  break;  
    case 6:
  ret = "mcg_odd";
  break;  
    default:
  ret = "unknown";
  break;
  }

  return ret;
}

const char* get_dslash_str(QudaDslashType type)
{
  const char* ret;
  
  switch( type){	
  case QUDA_WILSON_DSLASH:
    ret=  "wilson";
    break;
  case QUDA_CLOVER_WILSON_DSLASH:
    ret= "clover";
    break;
  case QUDA_CLOVER_HASENBUSCH_TWIST_DSLASH: ret = "clover-hasenbusch-twist"; break;
  case QUDA_TWISTED_MASS_DSLASH:
    ret= "twisted-mass";
    break;
  case QUDA_TWISTED_CLOVER_DSLASH:
    ret= "twisted-clover";
    break;
  case QUDA_STAGGERED_DSLASH:
    ret = "staggered";
    break;
  case QUDA_ASQTAD_DSLASH:
    ret = "asqtad";
    break;
  case QUDA_DOMAIN_WALL_DSLASH:
    ret = "domain-wall";
    break;
  case QUDA_DOMAIN_WALL_4D_DSLASH:
    ret = "domain_wall_4d";
    break;
  case QUDA_MOBIUS_DWF_DSLASH:
    ret = "mobius";
    break;
  case QUDA_LAPLACE_DSLASH:
    ret = "laplace";
    break;
  default:
    ret = "unknown";	
    break;
  }
  
  
  return ret;
    
}

const char *get_contract_str(QudaContractType type)
{
  const char *ret;

  switch (type) {
  case QUDA_CONTRACT_TYPE_OPEN: ret = "open"; break;
  case QUDA_CONTRACT_TYPE_DR: ret = "Degrand-Rossi"; break;
  default: ret = "unknown"; break;
  }

  return ret;
}

const char *get_eig_spectrum_str(QudaEigSpectrumType type)
{
  const char *ret;

  switch (type) {
  case QUDA_SPECTRUM_SR_EIG: ret = "SR"; break;
  case QUDA_SPECTRUM_LR_EIG: ret = "LR"; break;
  case QUDA_SPECTRUM_SM_EIG: ret = "SM"; break;
  case QUDA_SPECTRUM_LM_EIG: ret = "LM"; break;
  case QUDA_SPECTRUM_SI_EIG: ret = "SI"; break;
  case QUDA_SPECTRUM_LI_EIG: ret = "LI"; break;
  default: ret = "unknown eigenspectrum"; break;
  }

  return ret;
}

const char *get_eig_type_str(QudaEigType type)
{
  const char *ret;

  switch (type) {
  case QUDA_EIG_TR_LANCZOS: ret = "trlm"; break;
  case QUDA_EIG_IR_LANCZOS: ret = "irlm"; break;
  case QUDA_EIG_IR_ARNOLDI: ret = "iram"; break;
  default: ret = "unknown eigensolver"; break;
  }

  return ret;
}

const char*
get_mass_normalization_str(QudaMassNormalization type)
{
  const char *s;

  switch (type) {
  case QUDA_KAPPA_NORMALIZATION:
    s = "kappa";
    break;
  case QUDA_MASS_NORMALIZATION:
    s = "mass";
    break;
  case QUDA_ASYMMETRIC_MASS_NORMALIZATION:
    s = "asym-mass";
    break;
  default:
    fprintf(stderr, "Error: invalid mass normalization\n");
    exit(1);
  }

  return s;
}

const char *
get_matpc_str(QudaMatPCType type)
{
  const char* ret;

  switch(type) {
  case QUDA_MATPC_EVEN_EVEN:
    ret = "even-even";
    break;
  case QUDA_MATPC_ODD_ODD:
    ret = "odd-odd";
    break;
  case QUDA_MATPC_EVEN_EVEN_ASYMMETRIC:
    ret = "even-even-asym";
    break;
  case QUDA_MATPC_ODD_ODD_ASYMMETRIC:
    ret = "odd-odd-asym";
    break;
  default:
    fprintf(stderr, "Error: invalid matpc type %d\n", type);
    exit(1);
  }

  return ret;
}

const char *
get_solve_str(QudaSolveType type)
{
  const char* ret;

  switch(type) {
  case QUDA_DIRECT_SOLVE:
    ret = "direct";
    break;
  case QUDA_DIRECT_PC_SOLVE:
    ret = "direct-pc";
    break;
  case QUDA_NORMOP_SOLVE:
    ret = "normop";
    break;
  case QUDA_NORMOP_PC_SOLVE:
    ret = "normop-pc";
    break;
  case QUDA_NORMERR_SOLVE:
    ret = "normerr";
    break;
  case QUDA_NORMERR_PC_SOLVE:
    ret = "normerr-pc";
    break;
  default:
    fprintf(stderr, "Error: invalid solve type %d\n", type);
    exit(1);
  }

  return ret;
}


const char*
get_flavor_str(QudaTwistFlavorType type)
{
  const char* ret;
  
  switch(type) {
  case QUDA_TWIST_SINGLET:
    ret = "singlet";
    break;
  case QUDA_TWIST_DEG_DOUBLET:
    ret = "deg-doublet";
    break;
  case QUDA_TWIST_NONDEG_DOUBLET:
    ret = "nondeg-doublet";
    break;
  case QUDA_TWIST_NO:
    ret = "no";
    break;
  default:
    ret = "unknown";
    break;
  }

  return ret;
}

const char* 
get_solver_str(QudaInverterType type)
{
  const char* ret;
  
  switch(type){
  case QUDA_CG_INVERTER:
    ret = "cg";
    break;
  case QUDA_BICGSTAB_INVERTER:
    ret = "bicgstab";
    break;
  case QUDA_GCR_INVERTER:
    ret = "gcr";
    break;
  case QUDA_PCG_INVERTER:
    ret = "pcg";
    break;
  case QUDA_MPCG_INVERTER:
    ret = "mpcg";
    break;
  case QUDA_MPBICGSTAB_INVERTER:
    ret = "mpbicgstab";
    break;
  case QUDA_MR_INVERTER:
    ret = "mr";
    break;
  case QUDA_SD_INVERTER:
    ret = "sd";
    break;
  case QUDA_EIGCG_INVERTER:
    ret = "eigcg";
    break;
  case QUDA_INC_EIGCG_INVERTER:
    ret = "inc-eigcg";
    break;
  case QUDA_GMRESDR_INVERTER:
    ret = "gmresdr";
    break;
  case QUDA_GMRESDR_PROJ_INVERTER:
    ret = "gmresdr-proj";
    break;
  case QUDA_GMRESDR_SH_INVERTER:
    ret = "gmresdr-sh";
    break;
  case QUDA_FGMRESDR_INVERTER:
    ret = "fgmresdr";
    break;
  case QUDA_MG_INVERTER:
    ret= "mg";
    break;
  case QUDA_BICGSTABL_INVERTER:
    ret = "bicgstab-l";
    break;
  case QUDA_CGNE_INVERTER:
    ret = "cgne";
    break;
  case QUDA_CGNR_INVERTER:
    ret = "cgnr";
    break;
  case QUDA_CG3_INVERTER:
    ret = "cg3";
    break;
  case QUDA_CG3NE_INVERTER:
    ret = "cg3ne";
    break;
  case QUDA_CG3NR_INVERTER:
    ret = "cg3nr";
    break;
  case QUDA_CA_CG_INVERTER:
    ret = "ca-cg";
    break;
  case QUDA_CA_CGNE_INVERTER:
    ret = "ca-cgne";
    break;
  case QUDA_CA_CGNR_INVERTER:
    ret = "ca-cgnr";
    break;
  case QUDA_CA_GCR_INVERTER:
    ret = "ca-gcr";
    break;
  default:
    ret = "unknown";
    errorQuda("Error: invalid solver type %d\n", type);
    break;
  }

  return ret;
}

const char* 
get_quda_ver_str()
{
  static char vstr[32];
  int major_num = QUDA_VERSION_MAJOR;
  int minor_num = QUDA_VERSION_MINOR;
  int ext_num = QUDA_VERSION_SUBMINOR;
  sprintf(vstr, "%1d.%1d.%1d", 
	  major_num,
	  minor_num,
	  ext_num);
  return vstr;
}

const char *get_ritz_location_str(QudaFieldLocation type)
{
  const char *s;

  switch (type) {
  case QUDA_CPU_FIELD_LOCATION: s = "cpu"; break;
  case QUDA_CUDA_FIELD_LOCATION: s = "cuda"; break;
  default: fprintf(stderr, "Error: invalid location\n"); exit(1);
  }

  return s;
}

const char *get_memory_type_str(QudaMemoryType type)
{
  const char *s;

  switch (type) {
  case QUDA_MEMORY_DEVICE: s = "device"; break;
  case QUDA_MEMORY_PINNED: s = "pinned"; break;
  case QUDA_MEMORY_MAPPED: s = "mapped"; break;
  default: fprintf(stderr, "Error: invalid memory type\n"); exit(1);
  }

  return s;
}
