#ifndef SOLVER_H
#define SOLVER_H

#include <sys/types.h>

#include "defines.h"
#include "utils.h"
#include "block.h"
#include "kernels.h"
#include "interpolator.h"

template<class Derived>
class Solver {
public:

  typedef Block::IndexType        IndexType;
  typedef TrilinealInterpolator   InterpolateType;

  Solver(Block * block, const PrecisionType& Dt, const PrecisionType& Pdt) :
      pBlock(block),
      pBuffers(block->pBuffers),
      pFlags(block->pFlags),
      rDx(block->rDx),
      rIdx(1.0f/block->rDx),
      rI3dx(1.0f/(block->rDx*block->rDx*block->rDx)),
      rDt(Dt),
      rIdt(1.0f/Dt),
      rPdt(Pdt),
      rRo(block->rRo),
      rMu(block->rMu),
      rKa(block->rKa),
      rCC2(block->rCC2),
      rBW(block->rBW),
      rBWP(block->rBW/2),
      rX(block->rX),
      rY(block->rY),
      rZ(block->rZ),
      rNB(block->rNB),
      rNE(block->rNE),
      rDim(block->rDim) {
  }

  ~Solver() {
  }

  void applyBc(
      PrecisionType * buff,
      size_t * nodeList,
      size_t nodeListSize,
      size_t * normal,
      int bcType,
      size_t dim) {

    // Difference
    if(bcType == 0) {
      #pragma omp parallel for
      for(size_t n = 0; n < nodeListSize; n++) {
        size_t cell = nodeList[n];
        size_t prev = cell-(rZ+rBW)*(rY+rBW)*normal[2]-(rZ+rBW)*normal[1]-normal[0];
        size_t next = cell+(rZ+rBW)*(rY+rBW)*normal[2]+(rZ+rBW)*normal[1]+normal[0];

        for(size_t d = 0; d < dim; d++)
          buff[next*dim+d] = 2 * buff[cell*dim+d] - buff[prev*dim+d];
      }
    }

    // Copy
    if(bcType == 1) {
      #pragma omp parallel for
      for(size_t n = 0; n < nodeListSize; n++) {
        size_t cell = nodeList[n];
        size_t next = cell+(rZ+rBW)*(rY+rBW)*normal[2]+(rZ+rBW)*normal[1]+normal[0];

        for(size_t d = 0; d < dim; d++)
          buff[next*dim+d] = buff[cell*dim+d];
      }
    }

  }

  void copyAll(PrecisionType * buff, size_t dim) {

    #define INDEX(I,J,K) IndexType::GetIndex((I),(J),(K),pBlock->mPaddY,pBlock->mPaddZ)

    #pragma omp parallel for
    for(size_t a = 0; a < rY + rBW; a++) {
      for(size_t b = 0; b < rX + rBW; b++) {
        for(size_t d = 0; d < dim; d++) {
          buff[INDEX(0,a,b)*dim+d] = buff[INDEX(1,a,b)*dim+d];
          buff[INDEX(a,0,b)*dim+d] = buff[INDEX(a,1,b)*dim+d];
          buff[INDEX(a,b,0)*dim+d] = buff[INDEX(a,b,1)*dim+d];

          buff[INDEX(rX + rBW - 1,a,b)*dim+d] = buff[INDEX(rX + rBW - 2,a,b)*dim+d];
          buff[INDEX(a,rY + rBW - 1,b)*dim+d] = buff[INDEX(a,rY + rBW - 2,b)*dim+d];
          buff[INDEX(a,b,rZ + rBW - 1)*dim+d] = buff[INDEX(a,b,rZ + rBW - 2)*dim+d];
        }
      }
    }

    #undef INDEX

  }

  void copyLeft(PrecisionType * buff, size_t dim) {

    #define INDEX(I,J,K) IndexType::GetIndex((I),(J),(K),pBlock->mPaddY,pBlock->mPaddZ)

    #pragma omp parallel for
    for(size_t k = 0; k < rZ + rBW; k++) {
      for(size_t j = 0; j < rY + rBW; j++) {
        for(size_t d = 0; d < dim; d++) {
          buff[INDEX(0,j,k)*dim+d] = buff[INDEX(1,j,k)*dim+d];
        }
      }
    }

    #undef INDEX

  }

  void copyRight(PrecisionType * buff, size_t dim) {

    #define INDEX(I,J,K) IndexType::GetIndex((I),(J),(K),pBlock->mPaddY,pBlock->mPaddZ)

    #pragma omp parallel for
    for(size_t k = 0; k < rZ + rBW; k++) {
      for(size_t j = 0; j < rY + rBW; j++) {
        for(size_t d = 0; d < dim; d++) {
          buff[INDEX(rX + rBW - 1,j,k)*dim+d] = buff[INDEX(rX + rBW - 2,j,k)*dim+d];
        }
      }
    }

    #undef INDEX

  }

  void copyDown(PrecisionType * buff, size_t dim) {

    #define INDEX(I,J,K) IndexType::GetIndex((I),(J),(K),pBlock->mPaddY,pBlock->mPaddZ)

    #pragma omp parallel for
    for(size_t i = 0; i < rX + rBW; i++) {
      for(size_t k = 0; k < rZ + rBW; k++) {
        for(size_t d = 0; d < dim; d++) {
          buff[INDEX(i,0,k)*dim+d] = buff[INDEX(i,1,k)*dim+d];
        }
      }
    }

    #undef INDEX

  }

  void copyUp(PrecisionType * buff, size_t dim) {

    #define INDEX(I,J,K) IndexType::GetIndex((I),(J),(K),pBlock->mPaddY,pBlock->mPaddZ)

    #pragma omp parallel for
    for(size_t i = 0; i < rX + rBW; i++) {
      for(size_t k = 0; k < rZ + rBW; k++) {
        for(size_t d = 0; d < dim; d++) {
          buff[INDEX(i,rY + rBW - 1,k)*dim+d] = buff[INDEX(i,rY + rBW - 2,k)*dim+d];
        }
      }
    }

    #undef INDEX

  }

  void copyBack(PrecisionType * buff, size_t dim) {

    #define INDEX(I,J,K) IndexType::GetIndex((I),(J),(K),pBlock->mPaddY,pBlock->mPaddZ)

    #pragma omp parallel for
    for(size_t i = 0; i < rX + rBW; i++) {
      for(size_t j = 0; j < rY + rBW; j++) {
        for(size_t d = 0; d < dim; d++) {
          buff[INDEX(i,j,0)*dim+d] = buff[INDEX(i,j,1)*dim+d];
        }
      }
    }

    #undef INDEX

  }

  void copyFront(PrecisionType * buff, size_t dim) {

    #define INDEX(I,J,K) IndexType::GetIndex((I),(J),(K),pBlock->mPaddY,pBlock->mPaddZ)

    #pragma omp parallel for
    for(size_t i = 0; i < rX + rBW; i++) {
      for(size_t j = 0; j < rY + rBW; j++) {
        for(size_t d = 0; d < dim; d++) {
          buff[INDEX(i,j,rZ + rBW - 1)*dim+d] = buff[INDEX(i,j,rZ + rBW - 2)*dim+d];
        }
      }
    }

    #undef INDEX

  }

  void copyLeftToRight(PrecisionType * buff, size_t dim) {

    #define INDEX(I,J,K) IndexType::GetIndex((I),(J),(K),pBlock->mPaddY,pBlock->mPaddZ)

    #pragma omp parallel for
    for(size_t k = 0; k < rZ + rBW; k++) {
      for(size_t j = 0; j < rY + rBW; j++) {
        for(size_t d = 0; d < dim; d++) {
          buff[INDEX(0,j,k)*dim+d] = buff[INDEX(rX + rBW - 2,j,k)*dim+d];
        }
      }
    }

    #undef INDEX

  }

  void copyUpToDown(PrecisionType * buff, size_t dim) {

    #define INDEX(I,J,K) IndexType::GetIndex((I),(J),(K),pBlock->mPaddY,pBlock->mPaddZ)

    #pragma omp parallel for
    for(size_t k = 0; k < rZ + rBW; k++) {
      for(size_t j = 0; j < rY + rBW; j++) {
        for(size_t d = 0; d < dim; d++) {
          buff[INDEX(j,k,1)*dim+d] = buff[INDEX(j,k,rX + rBW - 2)*dim+d];
        }
      }
    }

    #undef INDEX

  }

  void Prepare() {
    static_cast<Derived*>(this)->Prepare_impl();
  }

  void Finish() {
    static_cast<Derived*>(this)->Finish_impl();
  }

  void Execute() {
    static_cast<Derived*>(this)->Execute_impl();
  }

  void ExecuteBlock() {
    static_cast<Derived*>(this)->ExecuteBlock_impl();
  }

  void ExecuteTask() {
    static_cast<Derived*>(this)->ExecuteTask_impl();
  }

protected:

  Block * pBlock;

  PrecisionType ** pBuffers;

  uint * pFlags;

  const PrecisionType & rDx;
  const PrecisionType rIdx;
  const PrecisionType rI3dx;
  const PrecisionType & rDt;
  const PrecisionType rIdt;
  const PrecisionType & rPdt;

  const PrecisionType & rRo;
  const PrecisionType & rMu;
  const PrecisionType & rKa;

  const PrecisionType & rCC2;

  const size_t &rBW;
  const size_t  rBWP;

  const size_t &rX;
  const size_t &rY;
  const size_t &rZ;

  const size_t &rNB;
  const size_t &rNE;

  const size_t &rDim;
};

#endif
