#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<fftw3.h>
#include<time.h>
#include<string.h>
#include <unistd.h>

#include "splinesurf.h"
#include "fourier.h"

/*
Program: Raman

Authors: Vinicius Vaz da Cruz - viniciusvcruz@gmail.com 
         Freddy Fernandes Guimarães
         

History: Based on FluxVE written by Vinicius

Purpose: Compute raman cross section from a eSPec wavepacket propagation

Method: Perform FFT from |f(R,r,t)> to |f(R,r,E)>, then choose a selected energy wavepacket to propagate in the final state

Goiania, 08th of december of 2014
 */

#define NAV   6.0221415E+23  //number of entities in 1 mol
#define Me    9.10938188E-31 //mass of electron in Kg
#define FSAU(x) (x)*41.3413745758e+0 ///0.024189E+0 //fs to au units of time conversion                        mtrxdiag_ (char *dim, int *il, int *iu, int *info, int *mxdct, int *n, double *abstol, int *iwork, int *np, double *eigvl, double *shm, double *vpot, double *work, double *wk, double *eigvc);

int rdinput(FILE *arq,char *dim,int *np,char *file,int *stfil,int *endfil, double *ti, double *tf, double *pti, double *ptf, double *pstept, double *m,char *potfile, int *nf,int *twopow, double *width, int *nEf,double *Ef, int *type, double *crosst, char *windtype,double *Ereso, double *shift,int *qop, char *qfnam,char *jobnam);

void readallspl(char *file,int *iflth,double *X,double *Y,double *T,double *bcoefRe,double *bcoefIm,double *xknot,double *yknot,double *tknot, int *np, int *nf, int *kx, int *stfil, char *dim);
  


int main(){
  int i,j,k,l,np[3],nf,nxr,nfr,xs,ys,ks,ldf,mdf,nw,nz,nwr,nzr,rdbcf;
  int fp[10],fpr[10],stfil,endfil,iflth,nrmse,type,nEf,nshift;
  int wholegrid,twopow,prtwpE,prtRMSwhole,qop;
  double norm;
  double ti,tf,pti,ptf,pstept,xi,xf,yi,yf,stept,sh[3],width,potshift,stepw,stepz;
  double *X,*Y,*T,rmse,xwork,*W,*Z,mem,Ereso;
  double m[3],x,y,pk,pki,pkf,steppk,ansk,val[5];
  char axis,file[30],potfile[30],wp_Enam[20],num[20],dim[6],windtype[10],fnam[20],fnam2[20],qfnam[50],jobnam[50];
  FILE *arq=fopen("raman.inp","r");
  FILE *deb=fopen("debug.dat","w");
  FILE *fqop;
  double *dipole;
  //--- spline variables
  int NTG,fpspl[4],ftinfo,kx;
  double *bcoefre,*bcoefim,*xknot,*yknot,*tknot;
  double stepxspl,stepyspl,steptspl,Xspl[3],Yspl[3],Respl[3],Imspl[3];
  //-- initial wavepacket parameters
  double crosst;
  //--- fourier variables
  int nE;
  double *WPERe,*WPEIm,*E,Ei,aE,stepE,*eknot,Ef[200],shift;
  fftw_complex *workin,*workout,*workk; //*aE;
  double kmin,kmax,Emin,Emax;
  fftw_plan p;
  clock_t begint,endt;
  double timediff;
  FILE *wp_E,*wp2_E;


  //--- Default values
  sprintf(jobnam,"default_jobname");
  type=0;
  np[1] = 1;
  nshift=0;
  shift=0.0e+0;
  rdbcf=1;
  wholegrid=1;
  stepxspl=1.0E-3;
  stepyspl=1.0E-3;
  twopow=9; //11;
  width = 1.0e-9;
  kx=6.0e+0;
  strcpy(windtype,"no input");
  qop = 1;
  prtwpE=1;       // =0 to print transformed wp into files -> NEED TO ADD TO rdinput
  prtRMSwhole=1; // = 0 to compute RMSE in relation to whole data -> NEED TO ADD TO rdinput

  //-------------------------------------------------//
  printf("\n =========================================\n");
  printf("||                                         ||\n");
  printf("||                  Raman                  ||\n");
  printf("||                                         ||\n");
  printf(" ==========================================\n");
  printf("Vinicius V. Cruz and Freddy F. Guimaraes\n\n");
  printf("Reading input parameters...\n\n");

  //--- Read Input File
  rdinput(arq,dim,np,file,&stfil,&endfil,&ti,&tf,&pti,&ptf,&pstept,m,potfile,&nf,&twopow,&width,&nEf,Ef,&type,&crosst,windtype,&Ereso,&shift,&qop,qfnam,jobnam);
  fclose(arq);
  //-----

  //--- Units Conversion
  //m1=(m1*1.0E-3)/(NAV * Me);
  //m2=(m2*1.0E-3)/(NAV * Me);
  //conversion below used in eSPec
 
  sh[0]=(yf -yi)/(np[0] -1.0);
  sh[1]=(xf -xi)/(np[1] -1.0);
  stept=(tf - ti)/(nf -1.0);
  for(i=0;i<3;i++)m[i] = 1.836152667539e+3*m[i];

  //NTG = pow(2,twopow+1);
  NTG = pow(2,twopow);
  steptspl=(2.0*tf)/NTG; //-1.0e+0);
  //steptspl=(tf - ti)/(NTG);
  //--- spline parameters determination 


  //--- Input Parameters -----------------------------------------------------------------------//
  printf("\n<< Starting input section >>\n\n");

  /*  printf("> Run type:\n");
  if(type==0){
    printf("This is a non-reactive calculation\n");
  }else if(type==1){
    printf("This is a reactive flux calculation\n");
    printf("a Jacobi coordinate transformation will be performed \n\n");
     wholegrid = 0;
     }*/
  printf("> Job name: %s \n",jobnam);
  printf(">Grid parameters:\n");
  printf("dimension: %s \n", dim);
  printf("npoints: %d ",np[0]);
  if(strncasecmp(dim,".2D",3)==0)printf("%d \n", np[1]);
  printf("\n");
  
  printf("\n>Wavepacket files:\n");
  printf("File name: %s\n",file);
  printf("ti %lf tf %lf step %lf fs nfile: %d \n",ti,tf,stept,nf);
  printf("masses m1 = %lf a.u.\n", m[0]);
  if(strncasecmp(dim,".2D",3)==0) printf("       m2 = %lf a.u.\n", m[1]);
  if(strncasecmp(dim,".2DCT",5)==0) printf("       m3 = %lf a.u.\n", m[2]);  

  printf("\n>Energy parameters\n");
  //printf("final state potential file: %s \n",potfile);
  //printf("propagation time: %lf %lf, step: %lf \n",pti,ptf,pstept);
  printf("shifted resonance frequency (Delta): %lf eV\n",Ereso*27.2114);
  printf("detuning: %lf \n",Ef[0]);
  for(k=1;k<nEf;k++) printf("          %lf\n",Ef[k]);
  printf("\n Energy values will be shifted by %lf a.u. \n",shift);

  if(qop == 0){
    printf("\n Transition operator will be read from file %s \n",qfnam);
    fqop=fopen(qfnam,"r");
    dipole=malloc(np[0]*np[1]*sizeof(double));
    //read dipole moments
    for(i=0;i<np[1];i++){
      if( strncasecmp(dim,".2D",3)==0 ) fscanf(fqop,"%lf",&xwork);
      for(j=0;j<np[0];j++){
	fscanf(fqop,"%lf %lf",&xwork,&dipole[i*np[0] + j]);
	//printf("%d %lf ",i*np[0] + j,dipole[i*np[0] + j]);
      }
    }
    //-----
  }

  
  printf("\n>Fourier transformparameters:\n");
  printf("grid is 2^%d, time step = %E \n",twopow,steptspl);
  stepE = 2*M_PI/(NTG*FSAU(steptspl));
  Ei = -NTG*stepE/(2.0E+0);
  nE = NTG;
  printf("Energy step = %E a.u., Ef = %E a.u. \n\n",stepE,-Ei);

  if(strncasecmp(windtype,".SGAUSS",7)==0){
    printf("Super gaussian window function will be applied\n");
    printf("width = %E \n",width);
  }else if(strncasecmp(windtype,".EXPDEC",7)==0){
    printf("Lifetime exponential decayment will be applied \n");
    printf("intermediate state lifetime = %E eV\n",width);
    width = width/(27.2114);
  }else{
    printf("invalid fourier transform window chosen\n error: %s \n",windtype);
    return 666;
  }
  
  
  printf("\nFinished input section!\n");
  printf("------------------------------------------------------\n\n\n");
 
  // convert time variables from fs to au
  ti = FSAU(ti);
  tf = FSAU(tf);
  stept = FSAU(stept);
  steptspl = FSAU(steptspl);

  //----- check if we already have splines available
  if( access( "fft_spline.bcoef", F_OK ) != -1 ) {
    // file exists
    printf("Previous spline coefficients found! \n");
    printf("the program will use this result to generate the wavepackets \n");
    printf("if you did not want to reuse a previous result, please erase the 'fft_spline.bcoef' file, and rerun the program.");
    rdbcf=0;
    goto finstep;
  }

  //--- Allocate arrays
  bcoefre = malloc(np[0]*np[1]*nf*sizeof(double));
  bcoefim = malloc(np[0]*np[1]*nf*sizeof(double));
  Y = malloc(np[0]*sizeof(double));
  X = malloc(np[1]*sizeof(double));
  T = malloc(nf*sizeof(double));
  xknot = malloc((np[1]+kx)*sizeof(double));
  yknot = malloc((np[0]+kx)*sizeof(double));
  tknot = malloc((nf+kx)*sizeof(double));
  workk = fftw_malloc(sizeof(fftw_complex) * NTG);
  WPERe = malloc(np[0]*np[1]*(NTG)*sizeof(double)); 
  WPEIm = malloc(np[0]*np[1]*(NTG)*sizeof(double)); 


  printf("\n<< Starting reading and spline section >>\n");

  //--- Read all wavepackets and generate spline coefficient matrices
  iflth = strlen(file);
  readallspl_(file,&iflth,X,Y,T,bcoefre,bcoefim,xknot,yknot,tknot,np,&nf,&kx,&stfil,dim);

  //------  spline debug
  //checkspl1d(Y,T,bcoefre,bcoefim,xknot,yknot,tknot,np,nf,kx);
  //------  
 

  printf("\n<< Starting Fourier section >>\n");

  begint = clock();
  ftinfo=run_all_fft(bcoefre,bcoefim,X,Y,xknot,yknot,tknot,kx,ti,tf,steptspl,np,nf,NTG,width,WPERe,WPEIm,windtype,dim);
  endt = clock();
  timediff = (double)(endt - begint)/CLOCKS_PER_SEC;

  stepE = 2*M_PI/(NTG*steptspl);
  Ei = -NTG*stepE/(2.0E+0);
  nE = NTG;
  
  if(ftinfo!=0){
    printf("\n An error occured while computing the wavepacket's Fourier transform: The program will stop!\n");
    return 666;
  }
  printf("\n Wavepacket has been trasformed to the Energy domain!\n");
  printf("\n it took %lf seconds \n",timediff);

  //checkmatrix_ (WPERe,WPEIm,&np[1],&np[0],&nE,X,Y,E);

  printf("\nGenerating spline representation of the fourier transformed wavepacket\n");
  free(bcoefre);
  free(bcoefim);
  //free(T); can I free T ???
  free(xknot);
  free(yknot);
  free(tknot);
  xknot = malloc((np[1]+kx)*sizeof(double));
  yknot = malloc((np[0]+kx)*sizeof(double));
  eknot = malloc((nE +kx)*sizeof(double));
  E = malloc(nE*sizeof(double));
  bcoefre = malloc(np[0]*np[1]*nE*sizeof(double));
  bcoefim = malloc(np[0]*np[1]*nE*sizeof(double));

  for(k=0;k<nE;k++){
    E[k] = Ei + k*stepE;
  }

  //---
  begint = clock();
  dbsnak_ (&np[0], Y, &kx, yknot);
  dbsnak_ (&nE, E, &kx, eknot);
  //----------
  if(strncasecmp(dim,".2D",3)==0){
    dbsnak_ (&np[1], X, &kx, xknot);
    dbs3in_ (&np[0],Y,&np[1],X,&nE,E,WPERe,&np[0],&np[1],&kx,&kx,&kx,yknot,xknot,eknot,bcoefre);
    dbs3in_ (&np[0],Y,&np[1],X,&nE,E,WPEIm,&np[0],&np[1],&kx,&kx,&kx,yknot,xknot,eknot,bcoefim);
  }else if(strncasecmp(dim,".1D",3)==0){
    dbs2in_ (&np[0],Y,&nE,E,WPERe,&np[0],&kx,&kx,yknot,eknot,bcoefre);
    dbs2in_ (&np[0],Y,&nE,E,WPEIm,&np[0],&kx,&kx,yknot,eknot,bcoefim);
  }

  endt = clock();
  timediff = (double)(endt - begint)/CLOCKS_PER_SEC;
  //----------

  free(WPERe);
  free(WPEIm);

  printf("Spline matrices calculated!\n");
  printf("it took %lf seconds\n",timediff);
  
  printf("\n Finished Fourier section\n");
  printf("------------------------------------------------------\n\n\n");

 
  //---
 finstep:
  //----------------- using previous spline calculation
  if(rdbcf==0){
    xknot = malloc((np[1]+kx)*sizeof(double));
    yknot = malloc((np[0]+kx)*sizeof(double));
    eknot = malloc((nE +kx)*sizeof(double));
    Y = malloc(np[0]*sizeof(double));
    X = malloc(np[1]*sizeof(double));
    E = malloc(nE*sizeof(double));
    bcoefre = malloc(np[0]*np[1]*nE*sizeof(double));
    bcoefim = malloc(np[0]*np[1]*nE*sizeof(double));
    printf("\nReading spline matrices from file \n");
    read_bcoef(jobnam,X,Y,E,bcoefre,bcoefim,np,nE);
    dbsnak_ (&np[0], Y, &kx, yknot);
    if(strncasecmp(dim,".2D",3)==0) dbsnak_ (&np[1], X, &kx, xknot);
    dbsnak_ (&nE, E, &kx, eknot);
    printf("Finished reading!\n");
  }//----------------

  printf("\n<< Starting initial conditions section >>\n");
  
  if(rdbcf==1){
    printf("spline coefficients matrices will be printed into files \n");
    print_bcoef(jobnam,X,Y,E,bcoefre,bcoefim,np,nE);
  }

  for(k=0;k<nEf;k++){

    while(Ef[k]/27.2114 + Ereso < Ei && k < nEf){
      printf("chosen detuning value %lf is out of the FFT range!\n",Ef[k]/27.2114 + Ereso);
      k = k + 1;
    }
    if(k > nEf) break;

    sprintf(fnam,"wp_%lf.dat",Ef[k]);
    printf("opening file %s \n",fnam);
    sprintf(fnam2,"wp2_%lf.dat",Ef[k]);

    wp_E = fopen(fnam,"w");
    printf("detun = %lf eV ,",Ef[k]);
    Ef[k] = Ef[k]/27.2114;
    printf("detun = %lf a.u. \n",Ef[k]);
    Ef[k] = Ef[k] + Ereso + shift;

    if(strncasecmp(dim,".2D",3)==0){

      // this second file is just for plotting on gnuplot
      
      wp2_E = fopen(fnam2,"w");

      norm = 0.0e+0;
      for(i=0;i<np[1];i++){
	for(j=0;j<np[0];j++){
	  if(qop==1){
	    val[0] = dbs3vl_ (&Y[j],&X[i],&Ef[k],&kx,&kx,&kx,yknot,xknot,eknot,&np[0],&np[1],&nE,bcoefre);
	    val[1] = dbs3vl_ (&Y[j],&X[i],&Ef[k],&kx,&kx,&kx,yknot,xknot,eknot,&np[0],&np[1],&nE,bcoefim);
	  }else{
	    val[0] = dipole[i*np[0] + j] * dbs3vl_ (&Y[j],&X[i],&Ef[k],&kx,&kx,&kx,yknot,xknot,eknot,&np[0],&np[1],&nE,bcoefre);
	    val[1] = dipole[i*np[0] + j] * dbs3vl_ (&Y[j],&X[i],&Ef[k],&kx,&kx,&kx,yknot,xknot,eknot,&np[0],&np[1],&nE,bcoefim);
	  }
	  norm = norm + val[0]*val[0] + val[1]*val[1];
	}
      }
      fprintf(wp_E,"# ominc = %E eV,  Intensity = %E \n",Ef[k]*27.2114,norm);
      for(i=0;i<np[1];i++){
	for(j=0;j<np[0];j++){
	  if(qop==1){
	    val[0] = (1.00/sqrt(norm))*dbs3vl_ (&Y[j],&X[i],&Ef[k],&kx,&kx,&kx,yknot,xknot,eknot,&np[0],&np[1],&nE,bcoefre);
	    val[1] = (1.00/sqrt(norm))*dbs3vl_ (&Y[j],&X[i],&Ef[k],&kx,&kx,&kx,yknot,xknot,eknot,&np[0],&np[1],&nE,bcoefim);
	  }else{
	    val[0] = (dipole[i*np[0] + j]/sqrt(norm))*dbs3vl_ (&Y[j],&X[i],&Ef[k],&kx,&kx,&kx,yknot,xknot,eknot,&np[0],&np[1],&nE,bcoefre);
	    val[1] = (dipole[i*np[0] + j]/sqrt(norm))*dbs3vl_ (&Y[j],&X[i],&Ef[k],&kx,&kx,&kx,yknot,xknot,eknot,&np[0],&np[1],&nE,bcoefim);
	  }
	  fprintf(wp_E,"%E %E %E %E \n",X[i],Y[j],val[0],val[1]);
	  fprintf(wp2_E,"%E %E %E %E \n",X[i],Y[j],val[0],val[1]);
	}
	fprintf(wp2_E,"\n");
      }
      fclose(wp2_E);
      //-----------------------------------
    }else if(strncasecmp(dim,".1D",3)==0){

      /* I don't think we can normalize the wavefunction for a given E separately...*/
      norm = 0.0e+0;
      for(j=0;j<np[0];j++){
	if(qop==1){
	  val[0] = dbs2vl_ (&Y[j],&Ef[k],&kx,&kx,yknot,eknot,&np[0],&nE,bcoefre);
	  val[1] = dbs2vl_ (&Y[j],&Ef[k],&kx,&kx,yknot,eknot,&np[0],&nE,bcoefim);
	}else{
	  val[0] = dipole[j] * dbs2vl_ (&Y[j],&Ef[k],&kx,&kx,yknot,eknot,&np[0],&nE,bcoefre);
	  val[1] = dipole[j] * dbs2vl_ (&Y[j],&Ef[k],&kx,&kx,yknot,eknot,&np[0],&nE,bcoefim);
	}
	norm = norm + val[0]*val[0] + val[1]*val[1];
      }
      fprintf(wp_E,"# ominc = %E eV,  Intensity = %E \n",Ef[k]*27.2114,norm);
      for(j=0;j<np[0];j++){
	if(qop==1){
	  val[0] = (1.00/sqrt(norm))*dbs2vl_ (&Y[j],&Ef[k],&kx,&kx,yknot,eknot,&np[0],&nE,bcoefre);
	  val[1] = (1.00/sqrt(norm))*dbs2vl_ (&Y[j],&Ef[k],&kx,&kx,yknot,eknot,&np[0],&nE,bcoefim);
	}else{
	  val[0] = (dipole[j]/sqrt(norm))*dbs2vl_ (&Y[j],&Ef[k],&kx,&kx,yknot,eknot,&np[0],&nE,bcoefre);
	  val[1] = (dipole[j]/sqrt(norm))*dbs2vl_ (&Y[j],&Ef[k],&kx,&kx,yknot,eknot,&np[0],&nE,bcoefim);
	}
	fprintf(wp_E,"%E %E %E \n",Y[j],val[0],val[1]);
      }
    }
    fclose(wp_E);
  } 
  
  printf("\n# End of Calculation!\n");
  printf("# Raman terminated successfully!\n");
  return 0;
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
