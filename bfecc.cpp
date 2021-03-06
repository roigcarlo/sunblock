#ifndef _WIN32
#include <sys/time.h>
#else
#include <Windows.h>
#endif

#include <sys/types.h>
#include <omp.h>

// Solver
#include "include/utils.h"
#include "include/block.h"
#include "include/defines.h"
#include "include/solver_stencil.h"
#include "include/solver_bfecc.h"
#include "include/file_io.h"
#include "include/interpolator.h"

#define WRITE_INIT_R(_STEP_)                                                        \
io.WriteGidMeshBin(dx,N,N,N);                                                       \
io.WriteGidResultsBin3D((PrecisionType*)buffers[AUX_3D_0],N,N,N,0,Dim,"AUX_3D_0");  \
io.WriteGidResultsBin3D((PrecisionType*)buffers[AUX_3D_1],N,N,N,0,Dim,"AUX_3D_1");  \
io.WriteGidResultsBin3D((PrecisionType*)buffers[AUX_3D_2],N,N,N,0,Dim,"velLappl");  \
io.WriteGidResultsBin3D((PrecisionType*)buffers[AUX_3D_3],N,N,N,0,Dim,"accelera");  \
io.WriteGidResultsBin3D((PrecisionType*)buffers[AUX_3D_4],N,N,N,0,Dim,"pressGra");  \
io.WriteGidResultsBin1D((PrecisionType*)buffers[AUX_3D_5],N,N,N,0    ,"VelDiver");  \
io.WriteGidResultsBin1D((PrecisionType*)buffers[AUX_3D_6],N,N,N,0    ,"PresDiff");  \
io.WriteGidResultsBin1D((PrecisionType*)buffers[AUX_3D_7],N,N,N,0    ,"PresLapp");  \
io.WriteGidResultsBin3D((PrecisionType*)buffers[VELOCITY],N,N,N,0,Dim,"velocity");  \
io.WriteGidResultsBin1D((PrecisionType*)buffers[PRESSURE],N,N,N,0    ,"pressure");  \

#define WRITE_RESULT(_STEP_)                                                            \
if (!(i%frec)) {                                                                        \
  io.WriteGidResultsBin3D((PrecisionType*)buffers[AUX_3D_0],N,N,N,i+1,Dim,"AUX_3D_0");  \
  io.WriteGidResultsBin3D((PrecisionType*)buffers[AUX_3D_1],N,N,N,i+1,Dim,"AUX_3D_1");  \
  io.WriteGidResultsBin3D((PrecisionType*)buffers[AUX_3D_2],N,N,N,i+1,Dim,"velLappl");  \
  io.WriteGidResultsBin3D((PrecisionType*)buffers[AUX_3D_3],N,N,N,i+1,Dim,"accelera");  \
  io.WriteGidResultsBin3D((PrecisionType*)buffers[AUX_3D_4],N,N,N,i+1,Dim,"pressGra");  \
  io.WriteGidResultsBin1D((PrecisionType*)buffers[AUX_3D_5],N,N,N,i+1    ,"VelDiver");  \
  io.WriteGidResultsBin1D((PrecisionType*)buffers[AUX_3D_6],N,N,N,i+1    ,"PresDiff");  \
  io.WriteGidResultsBin1D((PrecisionType*)buffers[AUX_3D_7],N,N,N,i+1    ,"PresLapp");  \
  io.WriteGidResultsBin3D((PrecisionType*)buffers[VELOCITY],N,N,N,i+1,Dim,"velocity");  \
  io.WriteGidResultsBin1D((PrecisionType*)buffers[PRESSURE],N,N,N,i+1    ,"pressure");  \
  OutputStep = _STEP_;                                                                  \
}                                                                                       \
OutputStep--;                                                                           \

PrecisionType calculateMaxDt_CFL(PrecisionType CFL, PrecisionType h, PrecisionType maxv) {
  return CFL*h / maxv;
}

PrecisionType calculateMaxDt_Fourier() {
  return 0.0;
}

PrecisionType calculatePressDt(PrecisionType dt, PrecisionType limit) {
  PrecisionType base = ceil(dt/limit);
  return dt/base;
}

int main(int argc, char *argv[]) {

#ifndef _WIN32
  struct timeval start, end;
#else
  int start, end;
#endif
  PrecisionType duration = 0.0;

  size_t N        = atoi(argv[1]);
  uint steeps     = atoi(argv[2]);
  size_t NB       = atoi(argv[4]);
  size_t NE       = (N+BW)/NB;
  uint OutputStep = 0;
  size_t Dim      = 3;
  uint frec       = steeps/10;

  PrecisionType h        = atof(argv[3]);
  PrecisionType omega    = 1.0f;
  PrecisionType maxv     = 0.0f;
  PrecisionType oldmaxv  = 0.0f;
  PrecisionType CFL      = 0.5f;

  PrecisionType dx       = h/(PrecisionType)N;
  PrecisionType dt       = 0.1f;
  PrecisionType pdt      = 0.1f;

  // air
  PrecisionType ro       = 1.0f;
  PrecisionType mu       = 1.9e-5;
  PrecisionType ka       = 1.0e-5f;
  PrecisionType cc       = 1.0f;
  PrecisionType cc2      = cc * cc;

  // water
  // PrecisionType ro       = 998.207f;
  // PrecisionType mu       = 1.002;
  // PrecisionType ka       = 1.0e-5f;
  // PrecisionType cc2      = 1481.0f*1481.0f;

  FileIO io("grid",N);

  Block         * block = NULL;

  int               NumBuffers = 20;
  PrecisionType **  buffers;

  buffers = (PrecisionType **)malloc(sizeof(PrecisionType * ) * NumBuffers);

  uint          * flags = NULL;

  MemManager memmrg(false);

  // Variable
  for( size_t i = 0; i < MAX_BUFF; i++) {
    memmrg.AllocateGrid(&buffers[i], N, N, N, 3, 1);
  }

  // Flags
  memmrg.AllocateGrid(&flags, N, N, N, 1, 1);

  for(uint i = 0; i < N+BW; i++)
    for(uint j = 0; j < N+BW; j++)
      for(uint k = 0; k < N+BW; k++)
        flags[k*(N+BW)*(N+BW)+j*(N+BW)+i] = 0;

  for(uint a = BWP; a < N+BWP; a++) {
    for(uint b = BWP; b < N+BWP; b++) {
      flags[a*(N+BW)*(N+BW)+b*(N+BW)+1] |= FIXED_VELOCITY_X;
      flags[a*(N+BW)*(N+BW)+b*(N+BW)+1] |= FIXED_VELOCITY_Y;
      flags[a*(N+BW)*(N+BW)+b*(N+BW)+1] |= FIXED_VELOCITY_Z;

      flags[a*(N+BW)*(N+BW)+b*(N+BW)+N] |= FIXED_VELOCITY_X;
      flags[a*(N+BW)*(N+BW)+b*(N+BW)+N] |= FIXED_VELOCITY_Y;
      flags[a*(N+BW)*(N+BW)+b*(N+BW)+N] |= FIXED_VELOCITY_Z;

      flags[a*(N+BW)*(N+BW)+1*(N+BW)+b] |= FIXED_VELOCITY_X;
      flags[a*(N+BW)*(N+BW)+1*(N+BW)+b] |= FIXED_VELOCITY_Y;
      flags[a*(N+BW)*(N+BW)+1*(N+BW)+b] |= FIXED_VELOCITY_Z;

      flags[a*(N+BW)*(N+BW)+N*(N+BW)+b] |= FIXED_VELOCITY_X;
      flags[a*(N+BW)*(N+BW)+N*(N+BW)+b] |= FIXED_VELOCITY_Y;
      flags[a*(N+BW)*(N+BW)+N*(N+BW)+b] |= FIXED_VELOCITY_Z;

      flags[1*(N+BW)*(N+BW)+a*(N+BW)+b] |= FIXED_VELOCITY_X;
      flags[1*(N+BW)*(N+BW)+a*(N+BW)+b] |= FIXED_VELOCITY_Y;
      flags[1*(N+BW)*(N+BW)+a*(N+BW)+b] |= FIXED_VELOCITY_Z;

      // flags[1*(N+BW)*(N+BW)+a*(N+BW)+b] |= FIXED_PRESSURE;
      // flags[N*(N+BW)*(N+BW)+a*(N+BW)+b] |= FIXED_PRESSURE;

      flags[N*(N+BW)*(N+BW)+a*(N+BW)+b] |= FIXED_VELOCITY_X;
      flags[N*(N+BW)*(N+BW)+a*(N+BW)+b] |= FIXED_VELOCITY_Y;
      flags[N*(N+BW)*(N+BW)+a*(N+BW)+b] |= FIXED_VELOCITY_Z;
    }
  }

  printf("Allocation correct\n");
  printf("Initialize\n");

  block = new Block(
    (PrecisionType**) buffers, (uint*) flags,
    dx, omega, ro, mu, ka, cc2, BW,
    N, N, N, NB, NE, Dim
  );

  block->Zero();
  printf("Zero\n");
  block->InitializeVelocity();
  printf("InitializeVelocity\n");
  block->InitializePressure();
  printf("InitializePressure\n");
  // block->WriteHeatFocus();
  printf("WriteHeatFocus\n");

  block->calculateMaxVelocity(maxv);
  dt = calculateMaxDt_CFL(CFL,dx,maxv);
  dt = 1.0f * std::min((h/N)/cc,(h/N)/maxv);
  // dt = calculateMaxDt_CFL(CFL,dx,maxv);

  printf(
    "Calculated dt: %f -- %f, %f, %f \n",
    dt,
    CFL,
    (PrecisionType)h/(PrecisionType)N,
    maxv);

  Solver<BfeccSolver>   AdvectionSolver(block,dt,pdt);
  Solver<StencilSolver> DiffusionSolver(block,dt,pdt);

  WRITE_INIT_R(frec)

  #pragma omp parallel
  #pragma omp single
  {
    printf("-------------------\n");
    printf("Running with OMP %d\n",omp_get_num_threads());
    printf("-------------------\n");
  }

#ifndef _WIN32
  gettimeofday(&start, NULL);
#else
  start = GetTickCount(); // At Program Start
#endif

  AdvectionSolver.Prepare();

  for (uint i = 0; i < steeps; i++) {

    double intergale;

    block->calculateRealMaxVelocity(intergale);

    oldmaxv = maxv;
    block->calculateMaxVelocity(maxv);
    dt = calculateMaxDt_CFL(CFL,dx,maxv);
    dt = 1.0f * 1.0f/(cc2*ro);
    dt = 1.0f * std::min((h/N)/cc,(h/N)/maxv);
    // dt = calculateMaxDt_CFL(CFL,dx,maxv);


    if (!(i%10000))
      printf("Step: %d\n",i);

    if (!(i%frec))
    printf(
      "Step %d: %f -- Seconds: %f, %f, MAXV: %f, [%f,%f] \n",
      i,
      dt,
      dt * i,
      (PrecisionType)h/(PrecisionType)N,
      intergale,
      (1.0f/64.0f)/dt,
      (maxv-oldmaxv));

    AdvectionSolver.Execute();
    DiffusionSolver.ExecuteTask();

    WRITE_RESULT(frec)
  }

  AdvectionSolver.Finish();

#ifndef _WIN32
  gettimeofday(&end, NULL);
  duration = FETCHTIME(start,end)
#else
  end = GetTickCount();
  duration = (end - start) / 1000.0f;
#endif
/*
  PrecisionType values_to_print[17] = {
    1.000,0.977,0.969,0.961,0.953,0.852,0.734,0.617,
    0.500,0.453,0.281,0.172,0.102,0.070,0.063,0.055,0.000
  };

  for(size_t i = 0; i < 17; i++) {

    PrecisionType values[3] = {0.0f, 0.0f, 0.0f};
    PrecisionType coords[3] = {h/2, h/2, values_to_print[i]};

    PrecisionType * velocity = buffers[VELOCITY];

    TrilinealInterpolator::Interpolate(block,velocity,values,coords,3);

    printf("%f\t%f\n",values_to_print[i],values[0]);
  }
*/
  printf("Total time:\t %f s\n",duration);
  printf("Step  time:\t %f s\n",duration/steeps);
  printf("Time per sec:\t %f s\n",duration/(steeps*dt));

  free(block);

  for( size_t i = 0; i < MAX_BUFF; i++) {
    memmrg.ReleaseGrid(&buffers[i], 1);
  }

  printf("De-Allocation correct\n");

  return 0;
}
