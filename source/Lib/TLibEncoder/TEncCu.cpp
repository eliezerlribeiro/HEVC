/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2014, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TEncCu.cpp
    \brief    Coding Unit (CU) encoder class
*/

#include <stdio.h>
#include "TEncTop.h"
#include "TEncCu.h"
#include "TEncAnalyze.h"
#include "TLibCommon/Debug.h"

#include "../../App/TAppEncoder/TAppEncTop.h"

#include <cmath>
#include <algorithm>
using namespace std;
extern int passou;
extern Bool ultimaLinha; 
extern int CTUcont;
extern int cpl_nCTB;

extern vector<int> maiorAltura;
extern vector<int> menorAltura;

extern vector<vector<int> > porcentagemCodificadaPorNivel;

extern vector<struct dadosWEKA> arvore64;
extern vector<struct dadosWEKA> arvore32;
extern vector<struct dadosWEKA> arvore16;
extern vector<struct dadosWEKA> arvore8;

int contadorDepth1=-1;
int contadorDepth2=-1;
int contadorDepth3=-1;

extern int CTU_hor;
extern int CTU_vert;

extern vector<vector<Bool> > split64Atual;
extern vector<vector<Bool> > split64Anterior;

extern vector<vector<Bool> > split32Atual;
extern vector<vector<Bool> > split32Anterior;

extern vector<vector<Bool> > split16Atual;
extern vector<vector<Bool> > split16Anterior;

//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

/**
 \param    uiTotalDepth  total number of allowable depth
 \param    uiMaxWidth    largest CU width
 \param    uiMaxHeight   largest CU height
 */
Void TEncCu::create(UChar uhTotalDepth, UInt uiMaxWidth, UInt uiMaxHeight, ChromaFormat chromaFormat)
{
  Int i;

  m_uhTotalDepth   = uhTotalDepth + 1;
  m_ppcBestCU      = new TComDataCU*[m_uhTotalDepth-1];
  m_ppcTempCU      = new TComDataCU*[m_uhTotalDepth-1];

  m_ppcPredYuvBest = new TComYuv*[m_uhTotalDepth-1];
  m_ppcResiYuvBest = new TComYuv*[m_uhTotalDepth-1];
  m_ppcRecoYuvBest = new TComYuv*[m_uhTotalDepth-1];
  m_ppcPredYuvTemp = new TComYuv*[m_uhTotalDepth-1];
  m_ppcResiYuvTemp = new TComYuv*[m_uhTotalDepth-1];
  m_ppcRecoYuvTemp = new TComYuv*[m_uhTotalDepth-1];
  m_ppcOrigYuv     = new TComYuv*[m_uhTotalDepth-1];

  UInt uiNumPartitions;
  for( i=0 ; i<m_uhTotalDepth-1 ; i++)
  {
    uiNumPartitions = 1<<( ( m_uhTotalDepth - i - 1 )<<1 );
    UInt uiWidth  = uiMaxWidth  >> i;
    UInt uiHeight = uiMaxHeight >> i;

    m_ppcBestCU[i] = new TComDataCU; m_ppcBestCU[i]->create( chromaFormat, uiNumPartitions, uiWidth, uiHeight, false, uiMaxWidth >> (m_uhTotalDepth - 1) );
    m_ppcTempCU[i] = new TComDataCU; m_ppcTempCU[i]->create( chromaFormat, uiNumPartitions, uiWidth, uiHeight, false, uiMaxWidth >> (m_uhTotalDepth - 1) );

    m_ppcPredYuvBest[i] = new TComYuv; m_ppcPredYuvBest[i]->create(uiWidth, uiHeight, chromaFormat);
    m_ppcResiYuvBest[i] = new TComYuv; m_ppcResiYuvBest[i]->create(uiWidth, uiHeight, chromaFormat);
    m_ppcRecoYuvBest[i] = new TComYuv; m_ppcRecoYuvBest[i]->create(uiWidth, uiHeight, chromaFormat);

    m_ppcPredYuvTemp[i] = new TComYuv; m_ppcPredYuvTemp[i]->create(uiWidth, uiHeight, chromaFormat);
    m_ppcResiYuvTemp[i] = new TComYuv; m_ppcResiYuvTemp[i]->create(uiWidth, uiHeight, chromaFormat);
    m_ppcRecoYuvTemp[i] = new TComYuv; m_ppcRecoYuvTemp[i]->create(uiWidth, uiHeight, chromaFormat);

    m_ppcOrigYuv    [i] = new TComYuv; m_ppcOrigYuv    [i]->create(uiWidth, uiHeight, chromaFormat);
  }

  m_bEncodeDQP          = false;
  m_CodeChromaQpAdjFlag = false;
  m_ChromaQpAdjIdc      = 0;

  // initialize partition order.
  UInt* piTmp = &g_auiZscanToRaster[0];
  initZscanToRaster( m_uhTotalDepth, 1, 0, piTmp);
  initRasterToZscan( uiMaxWidth, uiMaxHeight, m_uhTotalDepth );

  // initialize conversion matrix from partition index to pel
  initRasterToPelXY( uiMaxWidth, uiMaxHeight, m_uhTotalDepth );
}

Void TEncCu::destroy()
{
  Int i;

  for( i=0 ; i<m_uhTotalDepth-1 ; i++)
  {
    if(m_ppcBestCU[i])
    {
      m_ppcBestCU[i]->destroy();      delete m_ppcBestCU[i];      m_ppcBestCU[i] = NULL;
    }
    if(m_ppcTempCU[i])
    {
      m_ppcTempCU[i]->destroy();      delete m_ppcTempCU[i];      m_ppcTempCU[i] = NULL;
    }
    if(m_ppcPredYuvBest[i])
    {
      m_ppcPredYuvBest[i]->destroy(); delete m_ppcPredYuvBest[i]; m_ppcPredYuvBest[i] = NULL;
    }
    if(m_ppcResiYuvBest[i])
    {
      m_ppcResiYuvBest[i]->destroy(); delete m_ppcResiYuvBest[i]; m_ppcResiYuvBest[i] = NULL;
    }
    if(m_ppcRecoYuvBest[i])
    {
      m_ppcRecoYuvBest[i]->destroy(); delete m_ppcRecoYuvBest[i]; m_ppcRecoYuvBest[i] = NULL;
    }
    if(m_ppcPredYuvTemp[i])
    {
      m_ppcPredYuvTemp[i]->destroy(); delete m_ppcPredYuvTemp[i]; m_ppcPredYuvTemp[i] = NULL;
    }
    if(m_ppcResiYuvTemp[i])
    {
      m_ppcResiYuvTemp[i]->destroy(); delete m_ppcResiYuvTemp[i]; m_ppcResiYuvTemp[i] = NULL;
    }
    if(m_ppcRecoYuvTemp[i])
    {
      m_ppcRecoYuvTemp[i]->destroy(); delete m_ppcRecoYuvTemp[i]; m_ppcRecoYuvTemp[i] = NULL;
    }
    if(m_ppcOrigYuv[i])
    {
      m_ppcOrigYuv[i]->destroy();     delete m_ppcOrigYuv[i];     m_ppcOrigYuv[i] = NULL;
    }
  }
  if(m_ppcBestCU)
  {
    delete [] m_ppcBestCU;
    m_ppcBestCU = NULL;
  }
  if(m_ppcTempCU)
  {
    delete [] m_ppcTempCU;
    m_ppcTempCU = NULL;
  }

  if(m_ppcPredYuvBest)
  {
    delete [] m_ppcPredYuvBest;
    m_ppcPredYuvBest = NULL;
  }
  if(m_ppcResiYuvBest)
  {
    delete [] m_ppcResiYuvBest;
    m_ppcResiYuvBest = NULL;
  }
  if(m_ppcRecoYuvBest)
  {
    delete [] m_ppcRecoYuvBest;
    m_ppcRecoYuvBest = NULL;
  }
  if(m_ppcPredYuvTemp)
  {
    delete [] m_ppcPredYuvTemp;
    m_ppcPredYuvTemp = NULL;
  }
  if(m_ppcResiYuvTemp)
  {
    delete [] m_ppcResiYuvTemp;
    m_ppcResiYuvTemp = NULL;
  }
  if(m_ppcRecoYuvTemp)
  {
    delete [] m_ppcRecoYuvTemp;
    m_ppcRecoYuvTemp = NULL;
  }
  if(m_ppcOrigYuv)
  {
    delete [] m_ppcOrigYuv;
    m_ppcOrigYuv = NULL;
  }
}

/** \param    pcEncTop      pointer of encoder class
 */
Void TEncCu::init( TEncTop* pcEncTop )
{
  m_pcEncCfg           = pcEncTop;
  m_pcPredSearch       = pcEncTop->getPredSearch();
  m_pcTrQuant          = pcEncTop->getTrQuant();
  m_pcBitCounter       = pcEncTop->getBitCounter();
  m_pcRdCost           = pcEncTop->getRdCost();

  m_pcEntropyCoder     = pcEncTop->getEntropyCoder();
  m_pcCavlcCoder       = pcEncTop->getCavlcCoder();
  m_pcSbacCoder        = pcEncTop->getSbacCoder();
  m_pcBinCABAC         = pcEncTop->getBinCABAC();

  m_pppcRDSbacCoder    = pcEncTop->getRDSbacCoder();
  m_pcRDGoOnSbacCoder  = pcEncTop->getRDGoOnSbacCoder();

  m_pcRateCtrl         = pcEncTop->getRateCtrl();
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/** \param  rpcCU pointer of CU data class
 */
Void TEncCu::compressCU( TComDataCU*& rpcCU )
{
  // initialize CU data
  m_ppcBestCU[0]->initCU( rpcCU->getPic(), rpcCU->getAddr() );
  m_ppcTempCU[0]->initCU( rpcCU->getPic(), rpcCU->getAddr() );

  // analysis of CU
  DEBUG_STRING_NEW(sDebug)

  xCompressCU( m_ppcBestCU[0], m_ppcTempCU[0], 0 DEBUG_STRING_PASS_INTO(sDebug) );
  DEBUG_STRING_OUTPUT(std::cout, sDebug)

#if ADAPTIVE_QP_SELECTION
  if( m_pcEncCfg->getUseAdaptQpSelect() )
  {
    if(rpcCU->getSlice()->getSliceType()!=I_SLICE) //IIII
    {
      xLcuCollectARLStats( rpcCU);
    }
  }
#endif
}
/** \param  pcCU  pointer of CU data class
 */
Void TEncCu::encodeCU ( TComDataCU* pcCU )
{
  if ( pcCU->getSlice()->getPPS()->getUseDQP() )
  {
    setdQPFlag(true);
  }

  if ( pcCU->getSlice()->getUseChromaQpAdj() )
  {
    setCodeChromaQpAdjFlag(true);
  }

  // Encode CU data
  
  contadorDepth1=-1;
  contadorDepth2=-1;
  contadorDepth3=-1;
  xEncodeCU( pcCU, 0, 0 );
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================
/** Derive small set of test modes for AMP encoder speed-up
 *\param   rpcBestCU
 *\param   eParentPartSize
 *\param   bTestAMP_Hor
 *\param   bTestAMP_Ver
 *\param   bTestMergeAMP_Hor
 *\param   bTestMergeAMP_Ver
 *\returns Void
*/
#if AMP_ENC_SPEEDUP
#if AMP_MRG
Void TEncCu::deriveTestModeAMP (TComDataCU *&rpcBestCU, PartSize eParentPartSize, Bool &bTestAMP_Hor, Bool &bTestAMP_Ver, Bool &bTestMergeAMP_Hor, Bool &bTestMergeAMP_Ver)
#else
Void TEncCu::deriveTestModeAMP (TComDataCU *&rpcBestCU, PartSize eParentPartSize, Bool &bTestAMP_Hor, Bool &bTestAMP_Ver)
#endif
{
  if ( rpcBestCU->getPartitionSize(0) == SIZE_2NxN )
  {
    bTestAMP_Hor = true;
  }
  else if ( rpcBestCU->getPartitionSize(0) == SIZE_Nx2N )
  {
    bTestAMP_Ver = true;
  }
  else if ( rpcBestCU->getPartitionSize(0) == SIZE_2Nx2N && rpcBestCU->getMergeFlag(0) == false && rpcBestCU->isSkipped(0) == false )
  {
    bTestAMP_Hor = true;
    bTestAMP_Ver = true;
  }

#if AMP_MRG
  //! Utilizing the partition size of parent PU
  if ( eParentPartSize >= SIZE_2NxnU && eParentPartSize <= SIZE_nRx2N )
  {
    bTestMergeAMP_Hor = true;
    bTestMergeAMP_Ver = true;
  }

  if ( eParentPartSize == NUMBER_OF_PART_SIZES ) //! if parent is intra
  {
    if ( rpcBestCU->getPartitionSize(0) == SIZE_2NxN )
    {
      bTestMergeAMP_Hor = true;
    }
    else if ( rpcBestCU->getPartitionSize(0) == SIZE_Nx2N )
    {
      bTestMergeAMP_Ver = true;
    }
  }

  if ( rpcBestCU->getPartitionSize(0) == SIZE_2Nx2N && rpcBestCU->isSkipped(0) == false )
  {
    bTestMergeAMP_Hor = true;
    bTestMergeAMP_Ver = true;
  }

  if ( rpcBestCU->getWidth(0) == 64 )
  {
    bTestAMP_Hor = false;
    bTestAMP_Ver = false;
  }
#else
  //! Utilizing the partition size of parent PU
  if ( eParentPartSize >= SIZE_2NxnU && eParentPartSize <= SIZE_nRx2N )
  {
    bTestAMP_Hor = true;
    bTestAMP_Ver = true;
  }

  if ( eParentPartSize == SIZE_2Nx2N )
  {
    bTestAMP_Hor = false;
    bTestAMP_Ver = false;
  }
#endif
}
#endif


// ====================================================================================================================
// Protected member functions
// ====================================================================================================================
/** Compress a CU block recursively with enabling sub-LCU-level delta QP
 *\param   rpcBestCU
 *\param   rpcTempCU
 *\param   uiDepth
 *\returns Void
 *
 *- for loop of QP value to compress the current CU with all possible QP
*/
TCoeff tempCoeffSaida[ 32 * 32 ];
TCoeff tempBlockEntrada[ 32 * 32 ];
#if AMP_ENC_SPEEDUP
Void TEncCu::xCompressCU( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, UInt uiDepth DEBUG_STRING_FN_DECLARE(sDebug_), PartSize eParentPartSize )
#else
Void TEncCu::xCompressCU( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, UInt uiDepth )
#endif
{
  TComPic* pcPic = rpcBestCU->getPic();
  DEBUG_STRING_NEW(sDebug)

  // get Original YUV data from picture
  m_ppcOrigYuv[uiDepth]->copyFromPicYuv( pcPic->getPicYuvOrg(), rpcBestCU->getAddr(), rpcBestCU->getZorderIdxInCU() );

    // variable for Early CU determination
  Bool    bSubBranch = true;

  // variable for Cbf fast mode PU decision
  Bool    doNotBlockPu = true;
  Bool    earlyDetectionSkipMode = false;

  Bool bBoundary = false;
  UInt uiLPelX   = rpcBestCU->getCUPelX();
  UInt uiRPelX   = uiLPelX + rpcBestCU->getWidth(0)  - 1;
  UInt uiTPelY   = rpcBestCU->getCUPelY();
  UInt uiBPelY   = uiTPelY + rpcBestCU->getHeight(0) - 1;

  Int iBaseQP = xComputeQP( rpcBestCU, uiDepth );
  Int iMinQP;
  Int iMaxQP;
  Bool isAddLowestQP = false;

  const UInt numberValidComponents = rpcBestCU->getPic()->getNumberValidComponents();

  if( (g_uiMaxCUWidth>>uiDepth) >= rpcTempCU->getSlice()->getPPS()->getMinCuDQPSize() )
  {
    Int idQP = m_pcEncCfg->getMaxDeltaQP();
    iMinQP = Clip3( -rpcTempCU->getSlice()->getSPS()->getQpBDOffset(CHANNEL_TYPE_LUMA), MAX_QP, iBaseQP-idQP );
    iMaxQP = Clip3( -rpcTempCU->getSlice()->getSPS()->getQpBDOffset(CHANNEL_TYPE_LUMA), MAX_QP, iBaseQP+idQP );
  }
  else
  {
    iMinQP = rpcTempCU->getQP(0);
    iMaxQP = rpcTempCU->getQP(0);
  }

  if ( m_pcEncCfg->getUseRateCtrl() )
  {
    iMinQP = m_pcRateCtrl->getRCQP();
    iMaxQP = m_pcRateCtrl->getRCQP();
  }

  // transquant-bypass (TQB) processing loop variable initialisation ---

  const Int lowestQP = iMinQP; // For TQB, use this QP which is the lowest non TQB QP tested (rather than QP'=0) - that way delta QPs are smaller, and TQB can be tested at all CU levels.

  if ( (rpcTempCU->getSlice()->getPPS()->getTransquantBypassEnableFlag()) )
  {
    isAddLowestQP = true; // mark that the first iteration is to cost TQB mode.
    iMinQP = iMinQP - 1;  // increase loop variable range by 1, to allow testing of TQB mode along with other QPs
    if ( m_pcEncCfg->getCUTransquantBypassFlagForceValue() )
    {
      iMaxQP = iMinQP;
    }
  }

  // If slice start or slice end is within this cu...
  TComSlice * pcSlice = rpcTempCU->getPic()->getSlice(rpcTempCU->getPic()->getCurrSliceIdx());
  Bool bSliceStart = pcSlice->getSliceSegmentCurStartCUAddr()>rpcTempCU->getSCUAddr()&&pcSlice->getSliceSegmentCurStartCUAddr()<rpcTempCU->getSCUAddr()+rpcTempCU->getTotalNumPart();
  Bool bSliceEnd = (pcSlice->getSliceSegmentCurEndCUAddr()>rpcTempCU->getSCUAddr()&&pcSlice->getSliceSegmentCurEndCUAddr()<rpcTempCU->getSCUAddr()+rpcTempCU->getTotalNumPart());
  Bool bInsidePicture = ( uiRPelX < rpcBestCU->getSlice()->getSPS()->getPicWidthInLumaSamples() ) && ( uiBPelY < rpcBestCU->getSlice()->getSPS()->getPicHeightInLumaSamples() );
  // We need to split, so don't try these modes.
  
  // Arvores, resultado 1 boleano;
  //Eliezer
//mudar o codigo aqui
// CTU min e max expansão
// Quantidade (%) que foi codificada em 64,32,16,8
  
int split=1;
  
if(rpcBestCU->getSlice()->getSliceType() != I_SLICE){
  if(passou && uiDepth!=4){
  TComTrQuant pcTrQuant;
   int TAMANHO_BLOCO=32;
    switch(uiDepth){
     case 0:
       TAMANHO_BLOCO=64;
       break;
     case 1:
       TAMANHO_BLOCO=32;
       break;
     case 2:
       TAMANHO_BLOCO=16;
       break;
     case 3:
      TAMANHO_BLOCO=8;
      break;
    }
    if(uiDepth==0){
        memset (tempBlockEntrada,(TCoeff)0,sizeof(TCoeff)*TAMANHO_BLOCO/2);
        memset (tempCoeffSaida,(TCoeff)0,sizeof(TCoeff)*TAMANHO_BLOCO/2);
    }else{
        memset (tempBlockEntrada,(TCoeff)0,sizeof(TCoeff)*TAMANHO_BLOCO);
        memset (tempCoeffSaida,(TCoeff)0,sizeof(TCoeff)*TAMANHO_BLOCO);
    }
    
    int largura = pcPic->getPicYuvOrg()->getWidth(COMPONENT_Y) ;
    

   // Pel * vetor = pcPic->getPicYuvOrg()->getAddr(COMPONENT_Y, rpcBestCU->getAddr());
   
    Pel * vetor = pcPic->getPicYuvOrg()->getAddr(COMPONENT_Y, rpcBestCU->getAddr(), rpcBestCU->getZorderIdxInCU());
   
 
    long int media=0;
    int sobel=0;
    long int gradiente=0;
    int entropiaVetor[26];
    memset(entropiaVetor,(int)0,sizeof(int)*26);
   
    int CO[26][26];
    memset(CO,(int)0,sizeof(int)*26*26);
    
    
    for (int i = 0; i< TAMANHO_BLOCO; i ++){        
        for (int j = 0; j < TAMANHO_BLOCO; j++){
            
            if(j+1<largura)
                CO[(vetor[(i*largura)+j])/10] [(vetor[(i*largura)+j+1] )/ 10]++;
                
            int index = vetor[(i*largura )+ j]/10;
            entropiaVetor[index]++;
            media+=vetor[(i*largura )+ j];
            
            if(i>0 && j>0 && i<TAMANHO_BLOCO-1 && j<TAMANHO_BLOCO-1){//Gradiente
                int PH = (vetor[(i*largura )+ j]* 0) +
                        (vetor[(i*largura )+ j+1]* 0) +
                        (vetor[(i*largura )+ j-1] * 0) +
                        (vetor[((i+1)*largura )+ j+1]* 1) +
                        (vetor[((i+1)*largura )+ j-1] * 1) +
                        (vetor[((i+1)*largura )+ j] * 1) +
                        (vetor[((i-1)*largura )+ j-1] * -1) +
                        (vetor[((i-1)*largura )+ j+1] * -1) +
                        (vetor[((i-1)*largura )+ j] * -1) 
                        ;
                     int PV =(vetor[(i*largura )+ j]* 0) +
                        (vetor[((i-1)*largura )+ j] * 0) +
                        (vetor[((i+1)*largura )+ j] * 0) +
                        (vetor[((i+1)*largura )+ j+1]* 1) +
                        (vetor[((i-1)*largura )+ j+1] * 1) +
                        (vetor[(i*largura )+ j+1]* 1) +
                        (vetor[((i-1)*largura )+ j-1] * -1) +
                        (vetor[((i+1)*largura )+ j-1] * -1) +
                        (vetor[(i*largura )+ j-1] * -1) 
                        ;
               gradiente += sqrt(pow(PH , 2) +pow(PV ,2));
                //Homogeneidade
                int b =  std::abs ( (vetor[(i*largura )+ j]* 8) +
                        (vetor[((i+1)*largura )+ j+1]* -1) +
                        (vetor[((i-1)*largura )+ j-1] * -1) +
                        (vetor[((i-1)*largura )+ j+1] * -1) +
                        (vetor[((i+1)*largura )+ j-1] * -1) +
                        (vetor[((i-1)*largura )+ j] * -1) +
                        (vetor[((i+1)*largura )+ j] * -1) +
                        (vetor[((i)*largura )+ j-1] * -1) +
                        (vetor[((i)*largura )+ j+1] * -1) 
                        );
                if (b>150){//treshold
                       sobel++;
               }
            }
            if(uiDepth==0 && i%2==0 && j%2==0 ){
                tempBlockEntrada[(i/2*TAMANHO_BLOCO)+(j/2)]=vetor[(i*largura )+ j];
            }
            if(uiDepth!=0){
                tempBlockEntrada[(i*TAMANHO_BLOCO)+j]=vetor[(i*largura )+ j];
            }
        }
    }
    
    float COhomog=0 , COenergia=0 , COentropia=0, COcontraste=0;
    for(int i =0 ; i<26;i++){
        for(int j =0 ; j<26;j++){
            if(CO[i][j]!=0){
                COhomog += CO[i][j] / (1+pow(2,i-j));
                COenergia += CO[i][j]*CO[i][j];
                COentropia += CO[i][j]*log2(CO[i][j]);
                COcontraste +=(i-j)*(i-j)*CO[i][j];
            }
        }
    }	  
    media = media / (TAMANHO_BLOCO*TAMANHO_BLOCO);
    long int variancia=0;
    long double entropia=0;
    for (int i = 0; i< TAMANHO_BLOCO; i ++){
       for (int j = 0; j < TAMANHO_BLOCO; j++){
            int index = vetor[(i*largura )+ j]/10;
            long double probA= (entropiaVetor[index]/(pow(TAMANHO_BLOCO,2)));
            entropia += probA*log2(probA);
            variancia += pow(vetor[(i*largura )+ j]-media,2);        
        }
     }
    variancia=variancia/(TAMANHO_BLOCO*TAMANHO_BLOCO);
    
    if(uiDepth==0){
        pcTrQuant.xTrMxNEliezer( tempBlockEntrada, tempCoeffSaida, TAMANHO_BLOCO/2, TAMANHO_BLOCO/2 );
    }else{
        pcTrQuant.xTrMxNEliezer( tempBlockEntrada, tempCoeffSaida, TAMANHO_BLOCO, TAMANHO_BLOCO );
    }
    //Media AC linha 1 e Coluna 1
    //Media de todos ACS
    long int mediaLinhaAC =0;
    long int mediaColunaAC = 0;
    long int mediaTodosAC=0;
    mediaTodosAC-=tempCoeffSaida[0];
    //Foi feito para nao mudar a estrutura que eu tinha de TAMANHO DE BLOCO, 
    //Como a DCT usa somente até 32 então foi feito dessa maneira.
    if(uiDepth==0){
        for (int i = 0; i< TAMANHO_BLOCO/2; i ++){
            for (int j = 0; j < TAMANHO_BLOCO/2; j++){
                if(i==0 && j!=0)
                    mediaLinhaAC += tempCoeffSaida[i*TAMANHO_BLOCO/2 + j];
                if(j==0 && i!=0)
                    mediaColunaAC += tempCoeffSaida[i*TAMANHO_BLOCO/2 + j];
                mediaTodosAC += tempCoeffSaida[i*TAMANHO_BLOCO/2 + j];
            }
        }
    }else{
        for (int i = 0; i< TAMANHO_BLOCO; i ++){
            for (int j = 0; j < TAMANHO_BLOCO; j++){
                if(i==0 && j!=0)
                    mediaLinhaAC += tempCoeffSaida[i*TAMANHO_BLOCO + j];
                if(j==0 && i!=0)
                    mediaColunaAC += tempCoeffSaida[i*TAMANHO_BLOCO + j];
                mediaTodosAC += tempCoeffSaida[i*TAMANHO_BLOCO + j];
            }
        }
    }
  
  
   
  //inserir os dados nas arvores;
    if((int)uiDepth==0){
      arvore64[0].DC=tempCoeffSaida[0];
      arvore64[0].entropia = entropia;
      arvore64[0].gradiente = gradiente;
      arvore64[0].media = media;
      arvore64[0].mediaColunaAC = mediaColunaAC;
      arvore64[0].mediaLinhaAC = mediaLinhaAC;
      arvore64[0].mediaTodosAC = mediaTodosAC;
      arvore64[0].sobel = sobel;
      arvore64[0].variancia = variancia;
      
      arvore64[0].COhomog = COhomog;
      arvore64[0].COenergia = COenergia;
      arvore64[0].COentropia = COentropia;
      arvore64[0].COcontraste = COcontraste;
      
    }else if((int)uiDepth==1){
        arvore32[contadorDepth1].DC=tempCoeffSaida[0];
        arvore32[contadorDepth1].entropia = entropia;
        arvore32[contadorDepth1].gradiente = gradiente;
        arvore32[contadorDepth1].media = media;
        arvore32[contadorDepth1].mediaColunaAC = mediaColunaAC;
        arvore32[contadorDepth1].mediaLinhaAC = mediaLinhaAC;
        arvore32[contadorDepth1].mediaTodosAC = mediaTodosAC;
        arvore32[contadorDepth1].sobel = sobel;
        arvore32[contadorDepth1].variancia = variancia;
        
        arvore32[contadorDepth1].COhomog = COhomog;
        arvore32[contadorDepth1].COenergia = COenergia;
        arvore32[contadorDepth1].COentropia = COentropia;
        arvore32[contadorDepth1].COcontraste = COcontraste;
        
    }else if((int)uiDepth==2){
        arvore16[contadorDepth2].DC=tempCoeffSaida[0];
        arvore16[contadorDepth2].entropia = entropia;
        arvore16[contadorDepth2].gradiente = gradiente;
        arvore16[contadorDepth2].media = media;
        arvore16[contadorDepth2].mediaColunaAC = mediaColunaAC;
        arvore16[contadorDepth2].mediaLinhaAC = mediaLinhaAC;
        arvore16[contadorDepth2].mediaTodosAC = mediaTodosAC;
        arvore16[contadorDepth2].sobel = sobel;
        arvore16[contadorDepth2].variancia = variancia;
        
        arvore16[contadorDepth2].COhomog = COhomog;
        arvore16[contadorDepth2].COenergia = COenergia;
        arvore16[contadorDepth2].COentropia = COentropia;
        arvore16[contadorDepth2].COcontraste = COcontraste;
        
    }else if((int)uiDepth==3){
        arvore8[contadorDepth3].DC=tempCoeffSaida[0];
        arvore8[contadorDepth3].entropia = entropia;
        arvore8[contadorDepth3].gradiente = gradiente;
        arvore8[contadorDepth3].media = media;
        arvore8[contadorDepth3].mediaColunaAC = mediaColunaAC;
        arvore8[contadorDepth3].mediaLinhaAC = mediaLinhaAC;
        arvore8[contadorDepth3].mediaTodosAC = mediaTodosAC;
        arvore8[contadorDepth3].sobel = sobel;
        arvore8[contadorDepth3].variancia = variancia;
        
        arvore8[contadorDepth3].COhomog = COhomog;
        arvore8[contadorDepth3].COenergia = COenergia;
        arvore8[contadorDepth3].COentropia = COentropia;
        arvore8[contadorDepth3].COcontraste = COcontraste;
    }
    //Calculos SomaAnterior e somaAtual
    /* Soma 64x64*/
    
    int iCTU =CTUcont/CTU_hor;
    int jCTU =CTUcont%CTU_hor;
    
    int iCTU32 =(CTUcont/CTU_hor)*2;
    int jCTU32 =(CTUcont%CTU_hor)*2;
    
    int iCTU16 =(CTUcont/CTU_hor)*4;
    int jCTU16 =(CTUcont%CTU_hor)*4;
    
    if((int)uiDepth==0){
    int somaAtual=0;
    int somaAnterior=0;

    if(iCTU-1>=0){
        if(split64Atual[iCTU-1][jCTU]){
            somaAtual++;
        }
        if(jCTU-1>=0){
            if(split64Atual[iCTU-1][jCTU-1]) 
                somaAtual++;
        }
        if(jCTU+1<CTU_hor){
            if(split64Atual[iCTU-1][jCTU+1])
                somaAtual++;
        }
    }
    if(jCTU-1>=0){
      if(split64Atual[iCTU][jCTU-1])
                somaAtual++;
    }

    if(iCTU-1>=0){
        if(split64Anterior[iCTU-1][jCTU]){
            somaAnterior++;
        }
        if(jCTU-1>=0){
            if(split64Anterior[iCTU-1][jCTU-1])
                somaAnterior++;
        }
        if(jCTU+1<CTU_hor){
            if(split64Anterior[iCTU-1][jCTU+1])
                somaAnterior++;
        }
    }
    if(jCTU+1<CTU_hor){
        if(split64Anterior[iCTU][jCTU+1])
                somaAnterior++;
    }
    if(jCTU-1>=0){
      if(split64Atual[iCTU][jCTU-1])
        somaAnterior++;
      if(iCTU+1<CTU_vert)
          if(split64Atual[iCTU+1][jCTU-1])
            somaAnterior++;
    }
    if(split64Atual[iCTU][jCTU])
        somaAnterior++;
    if(iCTU+1<CTU_vert){
        if(split64Atual[iCTU+1][jCTU])
            somaAnterior++;
         if(jCTU+1<CTU_hor){
            if(split64Anterior[iCTU+1][jCTU+1])
                somaAnterior++;
        }
    }
    arvore64[0].vizinhosSplitAtual=somaAtual;
    arvore64[0].vizinhosSplitAnterior=somaAnterior;
    }else if((int)uiDepth==1){
    /* Soma 32x32*/
    
        int somaAtual32=0;
        int somaAnterior32=0;

        iCTU32=iCTU32+(contadorDepth1/2);
        jCTU32=jCTU32+(contadorDepth1%2);
        
        if(iCTU32-1>=0){
            if(split32Atual[iCTU32-1][jCTU32]){
                somaAtual32++;
            }
            if(jCTU32-1>=0){
                if(split32Atual[iCTU32-1][jCTU32-1]) 
                    somaAtual32++;
            }
            if(jCTU32+1<CTU_hor){
                if(split32Atual[iCTU32-1][jCTU32+1])
                    somaAtual32++;
            }
        }
        if(jCTU32-1>=0){
          if(split32Atual[iCTU32][jCTU32-1])
                    somaAtual32++;
        }

        if(iCTU32-1>=0){
            if(split32Anterior[iCTU32-1][jCTU32]){
                somaAnterior32++;
            }
            if(jCTU32-1>=0){
                if(split32Anterior[iCTU32-1][jCTU32-1])
                    somaAnterior32++;
            }
            if(jCTU32+1<CTU_hor){
                if(split32Anterior[iCTU32-1][jCTU32+1])
                    somaAnterior32++;
            }
        }
        if(jCTU32+1<CTU_hor){
            if(split32Anterior[iCTU32][jCTU32+1])
                    somaAnterior32++;
        }
        if(jCTU32-1>=0){
          if(split32Atual[iCTU32][jCTU32-1])
            somaAnterior32++;
          if(iCTU32+1<CTU_vert)
              if(split32Atual[iCTU32+1][jCTU32-1])
                 somaAnterior32++;
        }
        if(split32Atual[iCTU32][jCTU32])
            somaAnterior32++;
        if(iCTU32+1<CTU_vert){
            if(split32Atual[iCTU32+1][jCTU32])
                somaAnterior32++;
             if(jCTU32+1<CTU_hor){
                if(split32Anterior[iCTU32+1][jCTU32+1])
                    somaAnterior32++;
            }
        }

        arvore32[contadorDepth1].vizinhosSplitAtual=somaAtual32;
        arvore32[contadorDepth1].vizinhosSplitAnterior=somaAnterior32;
    }else if((int)uiDepth==2){
    /*Soma 16x16*/

  

    if(contadorDepth2==1){
        jCTU16=jCTU16+1;
    }else if(contadorDepth2==2){
        iCTU16=iCTU16+1;
    }else if(contadorDepth2==3){
        iCTU16=iCTU16+1;
        jCTU16=jCTU16+1;
    }else if(contadorDepth2==4){
        jCTU16=jCTU16+2;
    }else if(contadorDepth2==5){
        jCTU16=jCTU16+3;
    }else if(contadorDepth2==6){
        iCTU16=iCTU16+1;
        jCTU16=jCTU16+2;
    }else if(contadorDepth2==7){
        iCTU16=iCTU16+1;
        jCTU16=jCTU16+3;
    } else if(contadorDepth2==8){
        iCTU16=iCTU16+2;
    } else if(contadorDepth2==9){
        iCTU16=iCTU16+2;
        jCTU16=jCTU16+1;
    } else if(contadorDepth2==10){
        iCTU16=iCTU16+3;
    } else if(contadorDepth2==11){
        iCTU16=iCTU16+3;
        jCTU16=jCTU16+1;
    } else if(contadorDepth2==12){
        iCTU16=iCTU16+2;
        jCTU16=jCTU16+2;
    } else if(contadorDepth2==13){
        iCTU16=iCTU16+2;
        jCTU16=jCTU16+3;
    } else if(contadorDepth2==14){
        iCTU16=iCTU16+3;
        jCTU16=jCTU16+2;
    } else if(contadorDepth2==15){
        iCTU16=iCTU16+3;
        jCTU16=jCTU16+3;
    }      

    int somaAtual16=0;
    int somaAnterior16=0;

    if(iCTU16-1>=0){
        if(split16Atual[iCTU16-1][jCTU16]){
            somaAtual16++;
        }
        if(jCTU16-1>=0){
            if(split16Atual[iCTU16-1][jCTU16-1]) 
                somaAtual16++;
        }
        if(jCTU16+1<CTU_hor){
            if(split16Atual[iCTU16-1][jCTU16+1])
                somaAtual16++;
        }
    }
    if(jCTU16-1>=0){
      if(split16Atual[iCTU16][jCTU16-1])
                somaAtual16++;
    }

    if(iCTU16-1>=0){
        if(split16Anterior[iCTU16-1][jCTU16]){
            somaAnterior16++;
        }
        if(jCTU16-1>=0){
            if(split16Anterior[iCTU16-1][jCTU16-1])
                somaAnterior16++;
        }
        if(jCTU16+1<CTU_hor){
            if(split16Anterior[iCTU16-1][jCTU16+1])
                somaAnterior16++;
        }
    }
    if(jCTU16+1<CTU_hor){
        if(split16Anterior[iCTU16][jCTU16+1])
                somaAnterior16++;
    }
    if(jCTU16-1>=0){
      if(split16Atual[iCTU16][jCTU16-1])
        somaAnterior16++;
      if(iCTU16+1<CTU_vert)
          if(split16Atual[iCTU16+1][jCTU16-1])
              somaAnterior16++;
    }
    if(split16Atual[iCTU16][jCTU16])
        somaAnterior16++;
    if(iCTU16+1<CTU_vert){
        if(split16Atual[iCTU16+1][jCTU16])
            somaAnterior16++;
         if(jCTU16+1<CTU_hor){
            if(split16Anterior[iCTU16+1][jCTU16+1])
                somaAnterior16++;
        }
    }
    arvore16[contadorDepth2].vizinhosSplitAtual=somaAtual16;
    arvore16[contadorDepth2].vizinhosSplitAnterior=somaAnterior16;
    }
  }

 if(passou){
    if((int)uiDepth > maiorAltura[CTUcont])
       maiorAltura[CTUcont] = uiDepth;
  }
  
  /*Arvore de decisão RANDON */
  /* 64x 64 Arvore */
  if ((int)uiDepth==0){
      if(arvore64[0].vizinhosSplitAnterior <= 1){
          split=1;
      }else if(arvore64[0].vizinhosSplitAnterior <= 8){
          if(arvore64[0].sobel <= 786){
              split=1;
          }else if(arvore64[0].vizinhosSplitAtual <=0){
              split=0;
          }else if(arvore64[0].COenergia <= 2565130){
              if(arvore64[0].gradiente <= 1124124){
                  if(arvore64[0].sobel <= 3393){
                      if(arvore64[0].COcontraste <= 5992){
                          if(arvore64[0].vizinhosSplitAnterior <= 2){
                              if(arvore64[0].vizinhosSplitAnterior <= 5){
                                  if(arvore64[0].COcontraste <= 1874){
                                      split=1;
                                  }else if(arvore64[0].variancia <= 2113){
                                      split=0;
                                  }else {
                                      split=1;
                                  }
                              }else{
                                  split=1;
                              }
                          }
                      }else if(arvore64[0].vizinhosSplitAnterior <= 5){
                          if(arvore64[0].vizinhosSplitAtual <=2){
                              split=0;
                          }else{
                              split=1;
                          }
                      }else if( arvore64[0].vizinhosSplitAnterior <= 6){
                          split=1;
                      }else if(arvore64[0].COenergia <= 840088){
                          if(arvore64[0].COenergia <= 485936){
                              split=0;
                          }else{
                              split=1;
                          }
                      
                      }else {
                          split=0;
                      }
                  }else{
                      split=0;
                  }
              }else{
                  split=1;
              }
          }else{
              split =1;
          }
      
      }else{
          split=1;
      }
      
  }else if((int)uiDepth==1){
  /*Arvore 32x32*/
      if(arvore32[contadorDepth1].vizinhosSplitAnterior<= 2){
          if(arvore32[contadorDepth1].vizinhosSplitAnterior <=0){
              split=0;
          }else if(arvore32[contadorDepth1].gradiente <= 265440){
              if(arvore32[contadorDepth1].COenergia <= 264644){
                  if(arvore32[contadorDepth1].COenergia <= 35752){
                      if(arvore32[contadorDepth1].variancia <= 4145){
                        split=0;
                      }else{
                        split=1;
                      }
                  }else{
                      split=0;
                  }
              }else{
                  split=1;
              }
          }else{
              split=0;
          }
      }else if(arvore32[contadorDepth1].vizinhosSplitAnterior <= 5){
          if(arvore32[contadorDepth1].entropia <= -475.057){
              split=0;
          }else if(arvore32[contadorDepth1].vizinhosSplitAnterior <= 3){
              if(arvore32[contadorDepth1].COentropia <= 5111){
                  split=1;
              }else if(arvore32[contadorDepth1].COenergia <= 131898){
                  if(arvore32[contadorDepth1].variancia <= 3416){
                      if(arvore32[contadorDepth1].COentropia <= 5303.57){
                          split=1;
                      }else if(arvore32[contadorDepth1].media <= 63){
                          split=0;
                      }else if(arvore32[contadorDepth1].COcontraste <= 395){
                          split = 0;
                      }else if(arvore32[contadorDepth1].mediaColunaAC <= 2160){
                          split=1;
                      }else {
                          split=0;
                      }
                  }else {
                      split=0;
                  }
              }else {
                  split=1;
              }
          }else if(arvore32[contadorDepth1].DC <= 12437){
              split=1;
          }else if(arvore32[contadorDepth1].COentropia <= 5662.3){
              split=1;
          }else {
              split=0;
          }
          
      }else {
          split =1;
      }
  /*Arvore 16x16*/    
  }else if((int)uiDepth==2){
      if(arvore16[contadorDepth2].vizinhosSplitAnterior <= 0){
          split=0;
      }else if( arvore16[contadorDepth2].vizinhosSplitAnterior <= 2){
          if(arvore16[contadorDepth2].DC <= 20401){
              if(arvore16[contadorDepth2].COcontraste <= 20){
                  split=0;
              }else if(arvore16[contadorDepth2].COenergia <= 1852){
                   split=1;
              }else if(arvore16[contadorDepth2].COenergia <= 17354){
                  if(arvore16[contadorDepth2].media <= 45){
                      split=0;
                  }else if(arvore16[contadorDepth2].variancia <= 5014){
                      if(arvore16[contadorDepth2].COcontraste <= 88){
                          split=0;
                      }else if(arvore16[contadorDepth2].COentropia <= 1169.41){
                          if(arvore16[contadorDepth2].vizinhosSplitAnterior <= 1){
                              if(arvore16[contadorDepth2].COenergia <= 7066){
                                  if(arvore16[contadorDepth2].COcontraste <= 429){
                                      if(arvore16[contadorDepth2].COenergia <= 5362){
                                          if(arvore16[contadorDepth2].entropia <= -79.3497){
                                              split=0;
                                          }else {
                                              split=1;
                                          }
                                      }else{
                                          split=0;
                                      }
                                  }else {
                                      split=0;
                                  }
                              }else{
                                  split=1;
                              }
                          }else if(arvore16[contadorDepth2].mediaColunaAC <= 8810){
                              if(arvore16[contadorDepth2].COenergia <= 6800){
                                  if(arvore16[contadorDepth2].variancia <= 3625){
                                      if(arvore16[contadorDepth2].COcontraste <= 168){
                                          split=0;
                                      }else if(arvore16[contadorDepth2].COentropia <= 714.954){
                                          split=0;
                                      }else if(arvore16[contadorDepth2].COenergia <= 4948){
                                          if(arvore16[contadorDepth2].mediaColunaAC <= 763){
                                              split=1;
                                          }else{
                                              split=0;
                                          }
                                      }else{
                                          split=1;
                                      }
                                  }else{
                                      split=1;
                                  }
                              }else{
                                  split=0;
                              }
                          }else{
                              split=0;
                          }
                      }else{
                        split=0;
                      }
                  }else{
                      split=1;
                  }
              }else{
                split=1; 
              }
          }else{
              split=1;
          }
      }else{
          split=1;
      }
      
      
  }
  /*Fim arvore decisão*/
  
      
}else{//caso Intra
    split=0;
}
  
  if(!bSliceEnd && !bSliceStart && bInsidePicture )
  {
    for (Int iQP=iMinQP; iQP<=iMaxQP; iQP++)
    {
      const Bool bIsLosslessMode = isAddLowestQP && (iQP == iMinQP);

      if (bIsLosslessMode)
      {
        iQP = lowestQP;
      }

      m_ChromaQpAdjIdc = 0;
      if (pcSlice->getUseChromaQpAdj())
      {
        /* Pre-estimation of chroma QP based on input block activity may be performed
         * here, using for example m_ppcOrigYuv[uiDepth] */
        /* To exercise the current code, the index used for adjustment is based on
         * block position
         */
        Int lgMinCuSize = pcSlice->getSPS()->getLog2MinCodingBlockSize();
        m_ChromaQpAdjIdc = ((uiLPelX >> lgMinCuSize) + (uiTPelY >> lgMinCuSize)) % (pcSlice->getPPS()->getChromaQpAdjTableSize() + 1);
      }

      rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );

      // do inter modes, SKIP and 2Nx2N
      if( rpcBestCU->getSlice()->getSliceType() != I_SLICE )
      {
        // 2Nx2N
        if(m_pcEncCfg->getUseEarlySkipDetection())
        {
           if(!split){   
            xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2Nx2N DEBUG_STRING_PASS_INTO(sDebug) );
            rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );//by Competition for inter_2Nx2N
           }
        }
        // SKIP
        
            xCheckRDCostMerge2Nx2N( rpcBestCU, rpcTempCU DEBUG_STRING_PASS_INTO(sDebug), &earlyDetectionSkipMode );//by Merge for inter_2Nx2N
        rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
        
        if(!m_pcEncCfg->getUseEarlySkipDetection())
        {
          // 2Nx2N, NxN
          if(!split){   
          xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2Nx2N DEBUG_STRING_PASS_INTO(sDebug) );
          rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
          }
          if(m_pcEncCfg->getUseCbfFastMode())
          {
            doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
          }
        }
      }

      if (bIsLosslessMode) // Restore loop variable if lossless mode was searched.
      {
        iQP = iMinQP;
      }
    }

    if(!earlyDetectionSkipMode)
    {
      for (Int iQP=iMinQP; iQP<=iMaxQP; iQP++)
      {
        const Bool bIsLosslessMode = isAddLowestQP && (iQP == iMinQP); // If lossless, then iQP is irrelevant for subsequent modules.

        if (bIsLosslessMode)
        {
          iQP = lowestQP;
        }

        rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );

        // do inter modes, NxN, 2NxN, and Nx2N
        if( rpcBestCU->getSlice()->getSliceType() != I_SLICE )
        {
          // 2Nx2N, NxN
          if(!( (rpcBestCU->getWidth(0)==8) && (rpcBestCU->getHeight(0)==8) ))
          {
            if( uiDepth == g_uiMaxCUDepth - g_uiAddCUDepth && doNotBlockPu)
            {
          if(!split){     
                xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_NxN DEBUG_STRING_PASS_INTO(sDebug)   );
                rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
          }     
            }
          }

          if(doNotBlockPu)
          {   
           if(!split){ 
                xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_Nx2N DEBUG_STRING_PASS_INTO(sDebug)  );
            rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
           } 
            if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_Nx2N )
            {
              doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
            }
          }
          if(doNotBlockPu)
          {
            if(!split){
                xCheckRDCostInter      ( rpcBestCU, rpcTempCU, SIZE_2NxN DEBUG_STRING_PASS_INTO(sDebug)  );
                rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
            }
            if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxN)
            {
              doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
            }
          }

          //! Try AMP (SIZE_2NxnU, SIZE_2NxnD, SIZE_nLx2N, SIZE_nRx2N)
          if( pcPic->getSlice(0)->getSPS()->getAMPAcc(uiDepth) )
          {
#if AMP_ENC_SPEEDUP
            Bool bTestAMP_Hor = false, bTestAMP_Ver = false;

#if AMP_MRG
            Bool bTestMergeAMP_Hor = false, bTestMergeAMP_Ver = false;

            deriveTestModeAMP (rpcBestCU, eParentPartSize, bTestAMP_Hor, bTestAMP_Ver, bTestMergeAMP_Hor, bTestMergeAMP_Ver);
#else
            deriveTestModeAMP (rpcBestCU, eParentPartSize, bTestAMP_Hor, bTestAMP_Ver);
#endif

            //! Do horizontal AMP
            if ( bTestAMP_Hor )
            {
              if(doNotBlockPu)
              {
           if(!split){     
                    xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnU DEBUG_STRING_PASS_INTO(sDebug) );
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
           }     
                if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxnU )
                {
                  doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
                }
              }
              if(doNotBlockPu)
              {
            if(!split){    
                    xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnD DEBUG_STRING_PASS_INTO(sDebug) );
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
            }    
                if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxnD )
                {
                  doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
                }
              }
            }
#if AMP_MRG
            else if ( bTestMergeAMP_Hor )
            {
              if(doNotBlockPu)
              {
             if(!split){   
                    xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnU DEBUG_STRING_PASS_INTO(sDebug), true );
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
             }   
                if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxnU )
                {
                  doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
                }
              }
              if(doNotBlockPu)
              {
              if(!split){  
                    xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnD DEBUG_STRING_PASS_INTO(sDebug), true );
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
              }  
                if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxnD )
                {
                  doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
                }
              }
            }
#endif

            //! Do horizontal AMP
            if ( bTestAMP_Ver )
            {
              if(doNotBlockPu)
              {
               if(!split){ 
                    xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nLx2N DEBUG_STRING_PASS_INTO(sDebug) );
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
               } 
                if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_nLx2N )
                {
                  doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
                }
              }
              if(doNotBlockPu)
              {
                if(!split){
                    xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nRx2N DEBUG_STRING_PASS_INTO(sDebug) );
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                }
              }
            }
#if AMP_MRG
            else if ( bTestMergeAMP_Ver )
            {
              if(doNotBlockPu)
              {
                if(!split){
                    xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nLx2N DEBUG_STRING_PASS_INTO(sDebug), true );
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                }
                if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_nLx2N )
                {
                  doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
                }
              }
              if(doNotBlockPu)
              {
                if(!split){
                    xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nRx2N DEBUG_STRING_PASS_INTO(sDebug), true );
                    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
                }
              }
            }
#endif

#else
            xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnU );
            rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
            xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnD );
            rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
            xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nLx2N );
            rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );

            xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nRx2N );
            rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );

#endif
          }
        }

        // do normal intra modes
        // speedup for inter frames
        Double intraCost = 0.0;

        if((rpcBestCU->getSlice()->getSliceType() == I_SLICE)                                     ||
           (rpcBestCU->getCbf( 0, COMPONENT_Y  ) != 0)                                            ||
          ((rpcBestCU->getCbf( 0, COMPONENT_Cb ) != 0) && (numberValidComponents > COMPONENT_Cb)) ||
          ((rpcBestCU->getCbf( 0, COMPONENT_Cr ) != 0) && (numberValidComponents > COMPONENT_Cr))  ) // avoid very complex intra if it is unlikely
        {
          if(!split){
          xCheckRDCostIntra( rpcBestCU, rpcTempCU, intraCost, SIZE_2Nx2N DEBUG_STRING_PASS_INTO(sDebug) );
          rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
          }
          if( uiDepth == g_uiMaxCUDepth - g_uiAddCUDepth )
          {
            if( rpcTempCU->getWidth(0) > ( 1 << rpcTempCU->getSlice()->getSPS()->getQuadtreeTULog2MinSize() ) )
            {
              Double tmpIntraCost;
          if(!split){
              xCheckRDCostIntra( rpcBestCU, rpcTempCU, tmpIntraCost, SIZE_NxN DEBUG_STRING_PASS_INTO(sDebug)   );
              intraCost = std::min(intraCost, tmpIntraCost);
              rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
          }
           
            }
          }
        }

        // test PCM
        if(pcPic->getSlice(0)->getSPS()->getUsePCM()
          && rpcTempCU->getWidth(0) <= (1<<pcPic->getSlice(0)->getSPS()->getPCMLog2MaxSize())
          && rpcTempCU->getWidth(0) >= (1<<pcPic->getSlice(0)->getSPS()->getPCMLog2MinSize()) )
        {
          UInt uiRawBits = getTotalBits(rpcBestCU->getWidth(0), rpcBestCU->getHeight(0), rpcBestCU->getPic()->getChromaFormat(), g_bitDepth);
          UInt uiBestBits = rpcBestCU->getTotalBits();
          if((uiBestBits > uiRawBits) || (rpcBestCU->getTotalCost() > m_pcRdCost->calcRdCost(uiRawBits, 0)))
          {
          if(!split){
                xCheckIntraPCM (rpcBestCU, rpcTempCU);
            rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );
          }
           
          }
        }

        if (bIsLosslessMode) // Restore loop variable if lossless mode was searched.
        {
          iQP = iMinQP;
        }
      }
    }

    m_pcEntropyCoder->resetBits();
    m_pcEntropyCoder->encodeSplitFlag( rpcBestCU, 0, uiDepth, true );
    rpcBestCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // split bits
    rpcBestCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
    rpcBestCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcBestCU->getTotalBits(), rpcBestCU->getTotalDistortion() );

    // Early CU determination
    if( m_pcEncCfg->getUseEarlyCU() && rpcBestCU->isSkipped(0) ) // Faz um teste para ver se o custo de parar agora é melhor do que o melhor custo 
    {															//
      bSubBranch = false;
    }
    else
    {
      bSubBranch = true;
    }
  }
  else if(!(bSliceEnd && bInsidePicture))
  {
    bBoundary = true;
  }

  // copy orginal YUV samples to PCM buffer
  if( rpcBestCU->isLosslessCoded(0) && (rpcBestCU->getIPCMFlag(0) == false))
  {
    xFillPCMBuffer(rpcBestCU, m_ppcOrigYuv[uiDepth]);
  }

  if( (g_uiMaxCUWidth>>uiDepth) == rpcTempCU->getSlice()->getPPS()->getMinCuDQPSize() )
  {
    Int idQP = m_pcEncCfg->getMaxDeltaQP();
    iMinQP = Clip3( -rpcTempCU->getSlice()->getSPS()->getQpBDOffset(CHANNEL_TYPE_LUMA), MAX_QP, iBaseQP-idQP );
    iMaxQP = Clip3( -rpcTempCU->getSlice()->getSPS()->getQpBDOffset(CHANNEL_TYPE_LUMA), MAX_QP, iBaseQP+idQP );
  }
  else if( (g_uiMaxCUWidth>>uiDepth) > rpcTempCU->getSlice()->getPPS()->getMinCuDQPSize() )
  {
    iMinQP = iBaseQP;
    iMaxQP = iBaseQP;
  }
  else
  {
    Int iStartQP;
    if( pcPic->getCU( rpcTempCU->getAddr() )->getSliceSegmentStartCU(rpcTempCU->getZorderIdxInCU()) == pcSlice->getSliceSegmentCurStartCUAddr())
    {
      iStartQP = rpcTempCU->getQP(0);
    }
    else
    {
      UInt uiCurSliceStartPartIdx = pcSlice->getSliceSegmentCurStartCUAddr() % pcPic->getNumPartInCU() - rpcTempCU->getZorderIdxInCU();
      iStartQP = rpcTempCU->getQP(uiCurSliceStartPartIdx);
    }
    iMinQP = iStartQP;
    iMaxQP = iStartQP;
  }

  if ( m_pcEncCfg->getUseRateCtrl() )
  {
    iMinQP = m_pcRateCtrl->getRCQP();
    iMaxQP = m_pcRateCtrl->getRCQP();
  }

  if ( m_pcEncCfg->getCUTransquantBypassFlagForceValue() )
  {
    iMaxQP = iMinQP; // If all TUs are forced into using transquant bypass, do not loop here.
  }

  
  for (Int iQP=iMinQP; iQP<=iMaxQP; iQP++)// o que isso faz?
  {
    const Bool bIsLosslessMode = false; // False at this level. Next level down may set it to true.

    rpcTempCU->initEstData( uiDepth, iQP, bIsLosslessMode );

    if(split==0){
        split=0;
    }
    // further split
    if( (bSubBranch && uiDepth < g_uiMaxCUDepth - g_uiAddCUDepth)) //Early Termination
    //if( bSubBranch && uiDepth < g_uiMaxCUDepth - g_uiAddCUDepth )
    {
        
      UChar       uhNextDepth         = uiDepth+1;
      TComDataCU* pcSubBestPartCU     = m_ppcBestCU[uhNextDepth];
      TComDataCU* pcSubTempPartCU     = m_ppcTempCU[uhNextDepth];
      DEBUG_STRING_NEW(sTempDebug)

              
// decisão abaixo;
    if(( ultimaLinha || split ||rpcBestCU->getSlice()->getSliceType() == I_SLICE )){
      for ( UInt uiPartUnitIdx = 0; uiPartUnitIdx < 4; uiPartUnitIdx++ )// divide a CU em 4 partes
      {
        pcSubBestPartCU->initSubCU( rpcTempCU, uiPartUnitIdx, uhNextDepth, iQP );           // clear sub partition datas or init.
        pcSubTempPartCU->initSubCU( rpcTempCU, uiPartUnitIdx, uhNextDepth, iQP );           // clear sub partition datas or init.

        Bool bInSlice = pcSubBestPartCU->getSCUAddr()+pcSubBestPartCU->getTotalNumPart()>pcSlice->getSliceSegmentCurStartCUAddr()&&pcSubBestPartCU->getSCUAddr()<pcSlice->getSliceSegmentCurEndCUAddr();
        if(bInSlice && ( pcSubBestPartCU->getCUPelX() < pcSlice->getSPS()->getPicWidthInLumaSamples() ) && ( pcSubBestPartCU->getCUPelY() < pcSlice->getSPS()->getPicHeightInLumaSamples() ) )
        {
            if(passou){
                if((int)uiDepth==0){
                contadorDepth1++;
                }else if((int)uiDepth==1){
                    contadorDepth2++;
                }
            }  
          
          if ( 0 == uiPartUnitIdx) //initialize RD with previous depth buffer
          {
            m_pppcRDSbacCoder[uhNextDepth][CI_CURR_BEST]->load(m_pppcRDSbacCoder[uiDepth][CI_CURR_BEST]);
          }
          else
          {
            m_pppcRDSbacCoder[uhNextDepth][CI_CURR_BEST]->load(m_pppcRDSbacCoder[uhNextDepth][CI_NEXT_BEST]);
          }

#if AMP_ENC_SPEEDUP
          DEBUG_STRING_NEW(sChild)
          if ( !rpcBestCU->isInter(0) )
          {
            xCompressCU( pcSubBestPartCU, pcSubTempPartCU, uhNextDepth DEBUG_STRING_PASS_INTO(sChild), NUMBER_OF_PART_SIZES );
          }
          else
          {
            xCompressCU( pcSubBestPartCU, pcSubTempPartCU, uhNextDepth DEBUG_STRING_PASS_INTO(sChild), rpcBestCU->getPartitionSize(0) );
          }
          DEBUG_STRING_APPEND(sTempDebug, sChild)
#else
          xCompressCU( pcSubBestPartCU, pcSubTempPartCU, uhNextDepth );//bestPartCU do que?
#endif

          rpcTempCU->copyPartFrom( pcSubBestPartCU, uiPartUnitIdx, uhNextDepth );         // Keep best part data to current temporary data.
          xCopyYuv2Tmp( pcSubBestPartCU->getTotalNumPart()*uiPartUnitIdx, uhNextDepth );
        
        //dimininuir pois é uma chamda recursiva--;
           if(passou){
                if((int)uiDepth==0){
                contadorDepth1--;
                }else if((int)uiDepth==1){
                    contadorDepth2--;
                }
            }  
          
        }
        else {
            if(passou){//não vai dar split, logo é a menor altura e tbm é a altura que foi codificado. S?N?
                if((int)uiDepth==1){
                  contadorDepth2+=4;
                }
            }
            
            if (bInSlice){
                pcSubBestPartCU->copyToPic( uhNextDepth );
                rpcTempCU->copyPartFrom( pcSubBestPartCU, uiPartUnitIdx, uhNextDepth );
            }
        }
      }//fim for

      if( !bBoundary )
      {
        m_pcEntropyCoder->resetBits();
        m_pcEntropyCoder->encodeSplitFlag( rpcTempCU, 0, uiDepth, true );

        rpcTempCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // split bits
        rpcTempCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
      }
      rpcTempCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() );

      if( (g_uiMaxCUWidth>>uiDepth) == rpcTempCU->getSlice()->getPPS()->getMinCuDQPSize() && rpcTempCU->getSlice()->getPPS()->getUseDQP())
      {
        Bool hasResidual = false;
        for( UInt uiBlkIdx = 0; uiBlkIdx < rpcTempCU->getTotalNumPart(); uiBlkIdx ++)
        {
          if( ( pcPic->getCU( rpcTempCU->getAddr() )->getSliceSegmentStartCU(uiBlkIdx+rpcTempCU->getZorderIdxInCU()) == rpcTempCU->getSlice()->getSliceSegmentCurStartCUAddr() ) &&
              (     rpcTempCU->getCbf(uiBlkIdx, COMPONENT_Y)
                || (rpcTempCU->getCbf(uiBlkIdx, COMPONENT_Cb) && (numberValidComponents > COMPONENT_Cb))
                || (rpcTempCU->getCbf(uiBlkIdx, COMPONENT_Cr) && (numberValidComponents > COMPONENT_Cr)) ) )
          {
            hasResidual = true;
            break;
          }
        }//fim for

        UInt uiTargetPartIdx;
        if ( pcPic->getCU( rpcTempCU->getAddr() )->getSliceSegmentStartCU(rpcTempCU->getZorderIdxInCU()) != pcSlice->getSliceSegmentCurStartCUAddr() )
        {
          uiTargetPartIdx = pcSlice->getSliceSegmentCurStartCUAddr() % pcPic->getNumPartInCU() - rpcTempCU->getZorderIdxInCU();
        }
        else
        {
          uiTargetPartIdx = 0;
        }
        if ( hasResidual )
        {
#if !RDO_WITHOUT_DQP_BITS
          m_pcEntropyCoder->resetBits();
          m_pcEntropyCoder->encodeQP( rpcTempCU, uiTargetPartIdx, false );
          rpcTempCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // dQP bits
          rpcTempCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
          rpcTempCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() );
#endif

          Bool foundNonZeroCbf = false;
          rpcTempCU->setQPSubCUs( rpcTempCU->getRefQP( uiTargetPartIdx ), rpcTempCU, 0, uiDepth, foundNonZeroCbf );
          assert( foundNonZeroCbf );
        }
        else
        {
          rpcTempCU->setQPSubParts( rpcTempCU->getRefQP( uiTargetPartIdx ), 0, uiDepth ); // set QP to default QP
        }
      }

      m_pppcRDSbacCoder[uhNextDepth][CI_NEXT_BEST]->store(m_pppcRDSbacCoder[uiDepth][CI_TEMP_BEST]);

      Bool isEndOfSlice        = rpcBestCU->getSlice()->getSliceMode()==FIXED_NUMBER_OF_BYTES
                                 && (rpcBestCU->getTotalBits()>rpcBestCU->getSlice()->getSliceArgument()<<3);
      Bool isEndOfSliceSegment = rpcBestCU->getSlice()->getSliceSegmentMode()==FIXED_NUMBER_OF_BYTES
                                 && (rpcBestCU->getTotalBits()>rpcBestCU->getSlice()->getSliceSegmentArgument()<<3);
      if(isEndOfSlice||isEndOfSliceSegment)
      {
        if (m_pcEncCfg->getCostMode()==COST_MIXED_LOSSLESS_LOSSY_CODING)
          rpcBestCU->getTotalCost()=rpcTempCU->getTotalCost() + (1.0 / m_pcRdCost->getLambda());
        else
          rpcBestCU->getTotalCost()=rpcTempCU->getTotalCost()+1;
      }
      
      /* Decisão de split Eliézer*/
        if (rpcTempCU->getTotalCost() < rpcBestCU->getTotalCost()){		//Temp: CU dividida; Best: CU nao dividida
            if(passou){
                if((int)uiDepth==0){
                arvore64[0].Split=true;
                contadorDepth1++;
                }else if((int)uiDepth==1){
                    arvore32[contadorDepth1].Split=true;
                    contadorDepth2++;
                }else if((int)uiDepth==2){
                    arvore16[contadorDepth2].Split=true;
                }
            }
        }
        else if(passou){
                if((int)uiDepth < menorAltura[CTUcont]){
                  menorAltura[CTUcont] =uiDepth;
                }  
                porcentagemCodificadaPorNivel[CTUcont][(int)uiDepth]++;
                if((int)uiDepth==0){
                  arvore64[0].Split=false;
                }else if((int)uiDepth==1){
                  arvore32[contadorDepth1].Split=false;
                  contadorDepth2+=4;
                }else if((int)uiDepth==2){
                  arvore16[contadorDepth2].Split=false;
                }
            }
       /*fIM DECISÃO SPLIT*/
      
        }
      xCheckBestMode( rpcBestCU, rpcTempCU, uiDepth DEBUG_STRING_PASS_INTO(sDebug) DEBUG_STRING_PASS_INTO(sTempDebug) DEBUG_STRING_PASS_INTO(false) ); // RD compare current larger prediction
                                                                                       // with sub partitioned prediction.
    }
  }
  DEBUG_STRING_APPEND(sDebug_, sDebug);
  rpcBestCU->copyToPic(uiDepth);                                                     // Copy Best data to Picture for next partition prediction.
  
  xCopyYuv2Pic( rpcBestCU->getPic(), rpcBestCU->getAddr(), rpcBestCU->getZorderIdxInCU(), uiDepth, uiDepth, rpcBestCU, uiLPelX, uiTPelY );   // Copy Yuv data to picture Yuv
  if( bBoundary ||(bSliceEnd && bInsidePicture))
  {
    return;
  }
  
  // Assert if Best prediction mode is NONE
  // Selected mode's RD-cost must be not MAX_DOUBLE.
  assert( rpcBestCU->getPartitionSize ( 0 ) != NUMBER_OF_PART_SIZES       );
  assert( rpcBestCU->getPredictionMode( 0 ) != NUMBER_OF_PREDICTION_MODES );
  assert( rpcBestCU->getTotalCost     (   ) != MAX_DOUBLE                 );
}

/** finish encoding a cu and handle end-of-slice conditions
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth
 * \returns Void
 */
Void TEncCu::finishCU( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  TComPic* pcPic = pcCU->getPic();
  TComSlice * pcSlice = pcCU->getPic()->getSlice(pcCU->getPic()->getCurrSliceIdx());

  //Calculate end address
  UInt uiCUAddr = pcCU->getSCUAddr()+uiAbsPartIdx;

  UInt uiInternalAddress = pcPic->getPicSym()->getPicSCUAddr(pcSlice->getSliceSegmentCurEndCUAddr()-1) % pcPic->getNumPartInCU();
  UInt uiExternalAddress = pcPic->getPicSym()->getPicSCUAddr(pcSlice->getSliceSegmentCurEndCUAddr()-1) / pcPic->getNumPartInCU();
  UInt uiPosX = ( uiExternalAddress % pcPic->getFrameWidthInCU() ) * g_uiMaxCUWidth+ g_auiRasterToPelX[ g_auiZscanToRaster[uiInternalAddress] ];
  UInt uiPosY = ( uiExternalAddress / pcPic->getFrameWidthInCU() ) * g_uiMaxCUHeight+ g_auiRasterToPelY[ g_auiZscanToRaster[uiInternalAddress] ];
  UInt uiWidth = pcSlice->getSPS()->getPicWidthInLumaSamples();
  UInt uiHeight = pcSlice->getSPS()->getPicHeightInLumaSamples();
  while(uiPosX>=uiWidth||uiPosY>=uiHeight)
  {
    uiInternalAddress--;
    uiPosX = ( uiExternalAddress % pcPic->getFrameWidthInCU() ) * g_uiMaxCUWidth+ g_auiRasterToPelX[ g_auiZscanToRaster[uiInternalAddress] ];
    uiPosY = ( uiExternalAddress / pcPic->getFrameWidthInCU() ) * g_uiMaxCUHeight+ g_auiRasterToPelY[ g_auiZscanToRaster[uiInternalAddress] ];
  }
  uiInternalAddress++;
  if(uiInternalAddress==pcCU->getPic()->getNumPartInCU())
  {
    uiInternalAddress = 0;
    uiExternalAddress = pcPic->getPicSym()->getCUOrderMap(pcPic->getPicSym()->getInverseCUOrderMap(uiExternalAddress)+1);
  }
  UInt uiRealEndAddress = pcPic->getPicSym()->getPicSCUEncOrder(uiExternalAddress*pcPic->getNumPartInCU()+uiInternalAddress);

  // Encode slice finish
  Bool bTerminateSlice = false;
  if (uiCUAddr+(pcCU->getPic()->getNumPartInCU()>>(uiDepth<<1)) == uiRealEndAddress)
  {
    bTerminateSlice = true;
  }
  UInt uiGranularityWidth = g_uiMaxCUWidth;
  uiPosX = pcCU->getCUPelX() + g_auiRasterToPelX[ g_auiZscanToRaster[uiAbsPartIdx] ];
  uiPosY = pcCU->getCUPelY() + g_auiRasterToPelY[ g_auiZscanToRaster[uiAbsPartIdx] ];
  Bool granularityBoundary=((uiPosX+pcCU->getWidth(uiAbsPartIdx))%uiGranularityWidth==0||(uiPosX+pcCU->getWidth(uiAbsPartIdx)==uiWidth))
    &&((uiPosY+pcCU->getHeight(uiAbsPartIdx))%uiGranularityWidth==0||(uiPosY+pcCU->getHeight(uiAbsPartIdx)==uiHeight));

  if(granularityBoundary)
  {
    // The 1-terminating bit is added to all streams, so don't add it here when it's 1.
    if (!bTerminateSlice)
      m_pcEntropyCoder->encodeTerminatingBit( bTerminateSlice ? 1 : 0 );
  }

  Int numberOfWrittenBits = 0;
  if (m_pcBitCounter)
  {
    numberOfWrittenBits = m_pcEntropyCoder->getNumberOfWrittenBits();
  }

  // Calculate slice end IF this CU puts us over slice bit size.
  UInt iGranularitySize = pcCU->getPic()->getNumPartInCU();
  Int iGranularityEnd = ((pcCU->getSCUAddr()+uiAbsPartIdx)/iGranularitySize)*iGranularitySize;
  if(iGranularityEnd<=pcSlice->getSliceSegmentCurStartCUAddr())
  {
    iGranularityEnd+=max(iGranularitySize,(pcCU->getPic()->getNumPartInCU()>>(uiDepth<<1)));
  }
  // Set slice end parameter
  if(pcSlice->getSliceMode()==FIXED_NUMBER_OF_BYTES&&!pcSlice->getFinalized()&&pcSlice->getSliceBits()+numberOfWrittenBits>pcSlice->getSliceArgument()<<3)
  {
    pcSlice->setSliceSegmentCurEndCUAddr(iGranularityEnd);
    pcSlice->setSliceCurEndCUAddr(iGranularityEnd);
    return;
  }
  // Set dependent slice end parameter
  if(pcSlice->getSliceSegmentMode()==FIXED_NUMBER_OF_BYTES&&!pcSlice->getFinalized()&&pcSlice->getSliceSegmentBits()+numberOfWrittenBits > pcSlice->getSliceSegmentArgument()<<3)
  {
    pcSlice->setSliceSegmentCurEndCUAddr(iGranularityEnd);
    return;
  }
  if(granularityBoundary)
  {
    pcSlice->setSliceBits( (UInt)(pcSlice->getSliceBits() + numberOfWrittenBits) );
    pcSlice->setSliceSegmentBits(pcSlice->getSliceSegmentBits()+numberOfWrittenBits);
    if (m_pcBitCounter)
    {
      m_pcEntropyCoder->resetBits();
    }
  }
}

/** Compute QP for each CU
 * \param pcCU Target CU
 * \param uiDepth CU depth
 * \returns quantization parameter
 */
Int TEncCu::xComputeQP( TComDataCU* pcCU, UInt uiDepth )
{
  Int iBaseQp = pcCU->getSlice()->getSliceQp();
  Int iQpOffset = 0;
  if ( m_pcEncCfg->getUseAdaptiveQP() )
  {
    TEncPic* pcEPic = dynamic_cast<TEncPic*>( pcCU->getPic() );
    UInt uiAQDepth = min( uiDepth, pcEPic->getMaxAQDepth()-1 );
    TEncPicQPAdaptationLayer* pcAQLayer = pcEPic->getAQLayer( uiAQDepth );
    UInt uiAQUPosX = pcCU->getCUPelX() / pcAQLayer->getAQPartWidth();
    UInt uiAQUPosY = pcCU->getCUPelY() / pcAQLayer->getAQPartHeight();
    UInt uiAQUStride = pcAQLayer->getAQPartStride();
    TEncQPAdaptationUnit* acAQU = pcAQLayer->getQPAdaptationUnit();

    Double dMaxQScale = pow(2.0, m_pcEncCfg->getQPAdaptationRange()/6.0);
    Double dAvgAct = pcAQLayer->getAvgActivity();
    Double dCUAct = acAQU[uiAQUPosY * uiAQUStride + uiAQUPosX].getActivity();
    Double dNormAct = (dMaxQScale*dCUAct + dAvgAct) / (dCUAct + dMaxQScale*dAvgAct);
    Double dQpOffset = log(dNormAct) / log(2.0) * 6.0;
    iQpOffset = Int(floor( dQpOffset + 0.49999 ));
  }

  return Clip3(-pcCU->getSlice()->getSPS()->getQpBDOffset(CHANNEL_TYPE_LUMA), MAX_QP, iBaseQp+iQpOffset );
}

/** encode a CU block recursively
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth
 * \returns Void
 */   
Void TEncCu::xEncodeCU( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  TComPic* pcPic = pcCU->getPic();

  Bool bBoundary = false;
  UInt uiLPelX   = pcCU->getCUPelX() + g_auiRasterToPelX[ g_auiZscanToRaster[uiAbsPartIdx] ];
  UInt uiRPelX   = uiLPelX + (g_uiMaxCUWidth>>uiDepth)  - 1;
  UInt uiTPelY   = pcCU->getCUPelY() + g_auiRasterToPelY[ g_auiZscanToRaster[uiAbsPartIdx] ];
  UInt uiBPelY   = uiTPelY + (g_uiMaxCUHeight>>uiDepth) - 1;

  TComSlice * pcSlice = pcCU->getPic()->getSlice(pcCU->getPic()->getCurrSliceIdx());
  // If slice start is within this cu...
  Bool bSliceStart = pcSlice->getSliceSegmentCurStartCUAddr() > pcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx &&
    pcSlice->getSliceSegmentCurStartCUAddr() < pcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+( pcPic->getNumPartInCU() >> (uiDepth<<1) );
  // We need to split, so don't try these modes.
  if(!bSliceStart&&( uiRPelX < pcSlice->getSPS()->getPicWidthInLumaSamples() ) && ( uiBPelY < pcSlice->getSPS()->getPicHeightInLumaSamples() ) )
  {
    m_pcEntropyCoder->encodeSplitFlag( pcCU, uiAbsPartIdx, uiDepth );
  }
  else
  {
    bBoundary = true;
  }
  
  if( ( ( uiDepth < pcCU->getDepth( uiAbsPartIdx ) ) && ( uiDepth < (g_uiMaxCUDepth-g_uiAddCUDepth) ) ) || bBoundary )
  {
    UInt uiQNumParts = ( pcPic->getNumPartInCU() >> (uiDepth<<1) )>>2;
    if( (g_uiMaxCUWidth>>uiDepth) == pcCU->getSlice()->getPPS()->getMinCuDQPSize() && pcCU->getSlice()->getPPS()->getUseDQP())
    {
      setdQPFlag(true);
    }

    if( (g_uiMaxCUWidth>>uiDepth) == pcCU->getSlice()->getPPS()->getMinCuChromaQpAdjSize() && pcCU->getSlice()->getUseChromaQpAdj())
    {
      setCodeChromaQpAdjFlag(true);
    }

    for ( UInt uiPartUnitIdx = 0; uiPartUnitIdx < 4; uiPartUnitIdx++, uiAbsPartIdx+=uiQNumParts )
    {
      uiLPelX   = pcCU->getCUPelX() + g_auiRasterToPelX[ g_auiZscanToRaster[uiAbsPartIdx] ];
      uiTPelY   = pcCU->getCUPelY() + g_auiRasterToPelY[ g_auiZscanToRaster[uiAbsPartIdx] ];
      Bool bInSlice = pcCU->getSCUAddr()+uiAbsPartIdx+uiQNumParts>pcSlice->getSliceSegmentCurStartCUAddr()&&pcCU->getSCUAddr()+uiAbsPartIdx<pcSlice->getSliceSegmentCurEndCUAddr();
      if(bInSlice&&( uiLPelX < pcSlice->getSPS()->getPicWidthInLumaSamples() ) && ( uiTPelY < pcSlice->getSPS()->getPicHeightInLumaSamples() ) )
      {
        xEncodeCU( pcCU, uiAbsPartIdx, uiDepth+1 );
      }
    }
    return;
  } 
  
  if( (g_uiMaxCUWidth>>uiDepth) >= pcCU->getSlice()->getPPS()->getMinCuDQPSize() && pcCU->getSlice()->getPPS()->getUseDQP())
  {
    setdQPFlag(true);
  }

  if( (g_uiMaxCUWidth>>uiDepth) >= pcCU->getSlice()->getPPS()->getMinCuChromaQpAdjSize() && pcCU->getSlice()->getUseChromaQpAdj())
  {
    setCodeChromaQpAdjFlag(true);
  }

  if (pcCU->getSlice()->getPPS()->getTransquantBypassEnableFlag())
  {
    m_pcEntropyCoder->encodeCUTransquantBypassFlag( pcCU, uiAbsPartIdx );
  }

  if( !pcCU->getSlice()->isIntra() )
  {
    m_pcEntropyCoder->encodeSkipFlag( pcCU, uiAbsPartIdx );
  }

  if( pcCU->isSkipped( uiAbsPartIdx ) )
  {
    m_pcEntropyCoder->encodeMergeIndex( pcCU, uiAbsPartIdx );
    finishCU(pcCU,uiAbsPartIdx,uiDepth);
    return;
  }

  m_pcEntropyCoder->encodePredMode( pcCU, uiAbsPartIdx );
  m_pcEntropyCoder->encodePartSize( pcCU, uiAbsPartIdx, uiDepth );

  if (pcCU->isIntra( uiAbsPartIdx ) && pcCU->getPartitionSize( uiAbsPartIdx ) == SIZE_2Nx2N )
  {
    m_pcEntropyCoder->encodeIPCMInfo( pcCU, uiAbsPartIdx );

    if(pcCU->getIPCMFlag(uiAbsPartIdx))
    {
      // Encode slice finish
      finishCU(pcCU,uiAbsPartIdx,uiDepth);
      return;
    }
  }

  // prediction Info ( Intra : direction mode, Inter : Mv, reference idx )
  m_pcEntropyCoder->encodePredInfo( pcCU, uiAbsPartIdx );

  // Encode Coefficients
  Bool bCodeDQP = getdQPFlag();
  Bool codeChromaQpAdj = getCodeChromaQpAdjFlag();
  m_pcEntropyCoder->encodeCoeff( pcCU, uiAbsPartIdx, uiDepth, bCodeDQP, codeChromaQpAdj );  
  setCodeChromaQpAdjFlag( codeChromaQpAdj );
  setdQPFlag( bCodeDQP );

  // --- write terminating bit ---
  finishCU(pcCU,uiAbsPartIdx,uiDepth);
}

Int xCalcHADs8x8_ISlice(Pel *piOrg, Int iStrideOrg)
{
  Int k, i, j, jj;
  Int diff[64], m1[8][8], m2[8][8], m3[8][8], iSumHad = 0;

  for( k = 0; k < 64; k += 8 )
  {
    diff[k+0] = piOrg[0] ;
    diff[k+1] = piOrg[1] ;
    diff[k+2] = piOrg[2] ;
    diff[k+3] = piOrg[3] ;
    diff[k+4] = piOrg[4] ;
    diff[k+5] = piOrg[5] ;
    diff[k+6] = piOrg[6] ;
    diff[k+7] = piOrg[7] ;

    piOrg += iStrideOrg;
  }

  //horizontal
  for (j=0; j < 8; j++)
  {
    jj = j << 3;
    m2[j][0] = diff[jj  ] + diff[jj+4];
    m2[j][1] = diff[jj+1] + diff[jj+5];
    m2[j][2] = diff[jj+2] + diff[jj+6];
    m2[j][3] = diff[jj+3] + diff[jj+7];
    m2[j][4] = diff[jj  ] - diff[jj+4];
    m2[j][5] = diff[jj+1] - diff[jj+5];
    m2[j][6] = diff[jj+2] - diff[jj+6];
    m2[j][7] = diff[jj+3] - diff[jj+7];

    m1[j][0] = m2[j][0] + m2[j][2];
    m1[j][1] = m2[j][1] + m2[j][3];
    m1[j][2] = m2[j][0] - m2[j][2];
    m1[j][3] = m2[j][1] - m2[j][3];
    m1[j][4] = m2[j][4] + m2[j][6];
    m1[j][5] = m2[j][5] + m2[j][7];
    m1[j][6] = m2[j][4] - m2[j][6];
    m1[j][7] = m2[j][5] - m2[j][7];

    m2[j][0] = m1[j][0] + m1[j][1];
    m2[j][1] = m1[j][0] - m1[j][1];
    m2[j][2] = m1[j][2] + m1[j][3];
    m2[j][3] = m1[j][2] - m1[j][3];
    m2[j][4] = m1[j][4] + m1[j][5];
    m2[j][5] = m1[j][4] - m1[j][5];
    m2[j][6] = m1[j][6] + m1[j][7];
    m2[j][7] = m1[j][6] - m1[j][7];
  }

  //vertical
  for (i=0; i < 8; i++)
  {
    m3[0][i] = m2[0][i] + m2[4][i];
    m3[1][i] = m2[1][i] + m2[5][i];
    m3[2][i] = m2[2][i] + m2[6][i];
    m3[3][i] = m2[3][i] + m2[7][i];
    m3[4][i] = m2[0][i] - m2[4][i];
    m3[5][i] = m2[1][i] - m2[5][i];
    m3[6][i] = m2[2][i] - m2[6][i];
    m3[7][i] = m2[3][i] - m2[7][i];

    m1[0][i] = m3[0][i] + m3[2][i];
    m1[1][i] = m3[1][i] + m3[3][i];
    m1[2][i] = m3[0][i] - m3[2][i];
    m1[3][i] = m3[1][i] - m3[3][i];
    m1[4][i] = m3[4][i] + m3[6][i];
    m1[5][i] = m3[5][i] + m3[7][i];
    m1[6][i] = m3[4][i] - m3[6][i];
    m1[7][i] = m3[5][i] - m3[7][i];

    m2[0][i] = m1[0][i] + m1[1][i];
    m2[1][i] = m1[0][i] - m1[1][i];
    m2[2][i] = m1[2][i] + m1[3][i];
    m2[3][i] = m1[2][i] - m1[3][i];
    m2[4][i] = m1[4][i] + m1[5][i];
    m2[5][i] = m1[4][i] - m1[5][i];
    m2[6][i] = m1[6][i] + m1[7][i];
    m2[7][i] = m1[6][i] - m1[7][i];
  }

  for (i = 0; i < 8; i++)
  {
    for (j = 0; j < 8; j++)
    {
      iSumHad += abs(m2[i][j]);
    }
  }
  iSumHad -= abs(m2[0][0]);
  iSumHad =(iSumHad+2)>>2;
  return(iSumHad);
}

Int  TEncCu::updateLCUDataISlice(TComDataCU* pcCU, Int LCUIdx, Int width, Int height)
{
  Int  xBl, yBl;
  const Int iBlkSize = 8;

  Pel* pOrgInit   = pcCU->getPic()->getPicYuvOrg()->getAddr(COMPONENT_Y, pcCU->getAddr(), 0);
  Int  iStrideOrig = pcCU->getPic()->getPicYuvOrg()->getStride(COMPONENT_Y);
  Pel  *pOrg;

  Int iSumHad = 0;
  for ( yBl=0; (yBl+iBlkSize)<=height; yBl+= iBlkSize)
  {
    for ( xBl=0; (xBl+iBlkSize)<=width; xBl+= iBlkSize)
    {
      pOrg = pOrgInit + iStrideOrig*yBl + xBl;
      iSumHad += xCalcHADs8x8_ISlice(pOrg, iStrideOrig);
    }
  }
  return(iSumHad);
}

/** check RD costs for a CU block encoded with merge
 * \param rpcBestCU
 * \param rpcTempCU
 * \returns Void
 */
Void TEncCu::xCheckRDCostMerge2Nx2N( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU DEBUG_STRING_FN_DECLARE(sDebug), Bool *earlyDetectionSkipMode )
{
  assert( rpcTempCU->getSlice()->getSliceType() != I_SLICE );
  TComMvField  cMvFieldNeighbours[2 * MRG_MAX_NUM_CANDS]; // double length for mv of both lists
  UChar uhInterDirNeighbours[MRG_MAX_NUM_CANDS];
  Int numValidMergeCand = 0;
  const Bool bTransquantBypassFlag = rpcTempCU->getCUTransquantBypass(0);

  for( UInt ui = 0; ui < rpcTempCU->getSlice()->getMaxNumMergeCand(); ++ui )
  {
    uhInterDirNeighbours[ui] = 0;
  }
  UChar uhDepth = rpcTempCU->getDepth( 0 );
  rpcTempCU->setPartSizeSubParts( SIZE_2Nx2N, 0, uhDepth ); // interprets depth relative to LCU level
  rpcTempCU->getInterMergeCandidates( 0, 0, cMvFieldNeighbours,uhInterDirNeighbours, numValidMergeCand );

  Int mergeCandBuffer[MRG_MAX_NUM_CANDS];
  for( UInt ui = 0; ui < numValidMergeCand; ++ui )
  {
    mergeCandBuffer[ui] = 0;
  }

  Bool bestIsSkip = false;

  UInt iteration;
  if ( rpcTempCU->isLosslessCoded(0))
  {
    iteration = 1;
  }
  else
  {
    iteration = 2;
  }
  DEBUG_STRING_NEW(bestStr)

  for( UInt uiNoResidual = 0; uiNoResidual < iteration; ++uiNoResidual )
  {
    for( UInt uiMergeCand = 0; uiMergeCand < numValidMergeCand; ++uiMergeCand )
    {
      if(!(uiNoResidual==1 && mergeCandBuffer[uiMergeCand]==1))
      {
        if( !(bestIsSkip && uiNoResidual == 0) )
        {
          DEBUG_STRING_NEW(tmpStr)
          // set MC parameters
          rpcTempCU->setPredModeSubParts( MODE_INTER, 0, uhDepth ); // interprets depth relative to LCU level
          rpcTempCU->setCUTransquantBypassSubParts( bTransquantBypassFlag, 0, uhDepth );
          rpcTempCU->setChromaQpAdjSubParts( bTransquantBypassFlag ? 0 : m_ChromaQpAdjIdc, 0, uhDepth );
          rpcTempCU->setPartSizeSubParts( SIZE_2Nx2N, 0, uhDepth ); // interprets depth relative to LCU level
          rpcTempCU->setMergeFlagSubParts( true, 0, 0, uhDepth ); // interprets depth relative to LCU level
          rpcTempCU->setMergeIndexSubParts( uiMergeCand, 0, 0, uhDepth ); // interprets depth relative to LCU level
          rpcTempCU->setInterDirSubParts( uhInterDirNeighbours[uiMergeCand], 0, 0, uhDepth ); // interprets depth relative to LCU level
          rpcTempCU->getCUMvField( REF_PIC_LIST_0 )->setAllMvField( cMvFieldNeighbours[0 + 2*uiMergeCand], SIZE_2Nx2N, 0, 0 ); // interprets depth relative to rpcTempCU level
          rpcTempCU->getCUMvField( REF_PIC_LIST_1 )->setAllMvField( cMvFieldNeighbours[1 + 2*uiMergeCand], SIZE_2Nx2N, 0, 0 ); // interprets depth relative to rpcTempCU level

          // do MC
          m_pcPredSearch->motionCompensation ( rpcTempCU, m_ppcPredYuvTemp[uhDepth] );
          // estimate residual and encode everything
          m_pcPredSearch->encodeResAndCalcRdInterCU( rpcTempCU,
                                                     m_ppcOrigYuv    [uhDepth],
                                                     m_ppcPredYuvTemp[uhDepth],
                                                     m_ppcResiYuvTemp[uhDepth],
                                                     m_ppcResiYuvBest[uhDepth],
                                                     m_ppcRecoYuvTemp[uhDepth],
                                                     (uiNoResidual != 0) DEBUG_STRING_PASS_INTO(tmpStr) );

#ifdef DEBUG_STRING
          DebugInterPredResiReco(tmpStr, *(m_ppcPredYuvTemp[uhDepth]), *(m_ppcResiYuvBest[uhDepth]), *(m_ppcRecoYuvTemp[uhDepth]), DebugStringGetPredModeMask(rpcTempCU->getPredictionMode(0)));
#endif

          if ((uiNoResidual == 0) && (rpcTempCU->getQtRootCbf(0) == 0))
          {
            // If no residual when allowing for one, then set mark to not try case where residual is forced to 0
            mergeCandBuffer[uiMergeCand] = 1;
          }

          rpcTempCU->setSkipFlagSubParts( rpcTempCU->getQtRootCbf(0) == 0, 0, uhDepth );
          Int orgQP = rpcTempCU->getQP( 0 );
          xCheckDQP( rpcTempCU );
          xCheckBestMode(rpcBestCU, rpcTempCU, uhDepth DEBUG_STRING_PASS_INTO(bestStr) DEBUG_STRING_PASS_INTO(tmpStr));

          rpcTempCU->initEstData( uhDepth, orgQP, bTransquantBypassFlag );

          if( m_pcEncCfg->getUseFastDecisionForMerge() && !bestIsSkip )
          {
            bestIsSkip = rpcBestCU->getQtRootCbf(0) == 0;
          }
        }
      }
    }

    if(uiNoResidual == 0 && m_pcEncCfg->getUseEarlySkipDetection())
    {
      if(rpcBestCU->getQtRootCbf( 0 ) == 0)
      {
        if( rpcBestCU->getMergeFlag( 0 ))
        {
          *earlyDetectionSkipMode = true;
        }
        else if(m_pcEncCfg->getFastSearch() != SELECTIVE)
        {
          Int absoulte_MV=0;
          for ( UInt uiRefListIdx = 0; uiRefListIdx < 2; uiRefListIdx++ )
          {
            if ( rpcBestCU->getSlice()->getNumRefIdx( RefPicList( uiRefListIdx ) ) > 0 )
            {
              TComCUMvField* pcCUMvField = rpcBestCU->getCUMvField(RefPicList( uiRefListIdx ));
              Int iHor = pcCUMvField->getMvd( 0 ).getAbsHor();
              Int iVer = pcCUMvField->getMvd( 0 ).getAbsVer();
              absoulte_MV+=iHor+iVer;
            }
          }

          if(absoulte_MV == 0)
          {
            *earlyDetectionSkipMode = true;
          }
        }
      }
    }
  }
  DEBUG_STRING_APPEND(sDebug, bestStr)
}


#if AMP_MRG
Void TEncCu::xCheckRDCostInter( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, PartSize ePartSize DEBUG_STRING_FN_DECLARE(sDebug), Bool bUseMRG)
#else
Void TEncCu::xCheckRDCostInter( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, PartSize ePartSize )
#endif
{
  DEBUG_STRING_NEW(sTest)

  UChar uhDepth = rpcTempCU->getDepth( 0 );

  rpcTempCU->setDepthSubParts( uhDepth, 0 );

  rpcTempCU->setSkipFlagSubParts( false, 0, uhDepth );

  rpcTempCU->setPartSizeSubParts  ( ePartSize,  0, uhDepth );
  rpcTempCU->setPredModeSubParts  ( MODE_INTER, 0, uhDepth );
  rpcTempCU->setChromaQpAdjSubParts( rpcTempCU->getCUTransquantBypass(0) ? 0 : m_ChromaQpAdjIdc, 0, uhDepth );

#if AMP_MRG
  rpcTempCU->setMergeAMP (true);
  m_pcPredSearch->predInterSearch ( rpcTempCU, m_ppcOrigYuv[uhDepth], m_ppcPredYuvTemp[uhDepth], m_ppcResiYuvTemp[uhDepth], m_ppcRecoYuvTemp[uhDepth] DEBUG_STRING_PASS_INTO(sTest), false, bUseMRG );
#else
  m_pcPredSearch->predInterSearch ( rpcTempCU, m_ppcOrigYuv[uhDepth], m_ppcPredYuvTemp[uhDepth], m_ppcResiYuvTemp[uhDepth], m_ppcRecoYuvTemp[uhDepth] );
#endif

#if AMP_MRG
  if ( !rpcTempCU->getMergeAMP() )
  {
    return;
  }
#endif

  m_pcPredSearch->encodeResAndCalcRdInterCU( rpcTempCU, m_ppcOrigYuv[uhDepth], m_ppcPredYuvTemp[uhDepth], m_ppcResiYuvTemp[uhDepth], m_ppcResiYuvBest[uhDepth], m_ppcRecoYuvTemp[uhDepth], false DEBUG_STRING_PASS_INTO(sTest) );
  rpcTempCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() );

#ifdef DEBUG_STRING
  DebugInterPredResiReco(sTest, *(m_ppcPredYuvTemp[uhDepth]), *(m_ppcResiYuvBest[uhDepth]), *(m_ppcRecoYuvTemp[uhDepth]), DebugStringGetPredModeMask(rpcTempCU->getPredictionMode(0)));
#endif

  xCheckDQP( rpcTempCU );
  xCheckBestMode(rpcBestCU, rpcTempCU, uhDepth DEBUG_STRING_PASS_INTO(sDebug) DEBUG_STRING_PASS_INTO(sTest));
}

Void TEncCu::xCheckRDCostIntra( TComDataCU *&rpcBestCU,
                                TComDataCU *&rpcTempCU,
                                Double      &cost,
                                PartSize     eSize
                                DEBUG_STRING_FN_DECLARE(sDebug) )
{
  DEBUG_STRING_NEW(sTest)

  UInt uiDepth = rpcTempCU->getDepth( 0 );

  rpcTempCU->setSkipFlagSubParts( false, 0, uiDepth );

  rpcTempCU->setPartSizeSubParts( eSize, 0, uiDepth );
  rpcTempCU->setPredModeSubParts( MODE_INTRA, 0, uiDepth );
  rpcTempCU->setChromaQpAdjSubParts( rpcTempCU->getCUTransquantBypass(0) ? 0 : m_ChromaQpAdjIdc, 0, uiDepth );

  Bool bSeparateLumaChroma = true; // choose estimation mode

  Distortion uiPreCalcDistC = 0;
  if (rpcBestCU->getPic()->getChromaFormat()==CHROMA_400)
  {
    bSeparateLumaChroma=true;
  }

  Pel resiLuma[NUMBER_OF_STORED_RESIDUAL_TYPES][MAX_CU_SIZE * MAX_CU_SIZE];

  if( !bSeparateLumaChroma )
  {
    // after this function, the direction will be PLANAR, DC, HOR or VER
    // however, if Luma ends up being one of those, the chroma dir must be later changed to DM_CHROMA.
    m_pcPredSearch->preestChromaPredMode( rpcTempCU, m_ppcOrigYuv[uiDepth], m_ppcPredYuvTemp[uiDepth] );
  }
  m_pcPredSearch->estIntraPredQT( rpcTempCU, m_ppcOrigYuv[uiDepth], m_ppcPredYuvTemp[uiDepth], m_ppcResiYuvTemp[uiDepth], m_ppcRecoYuvTemp[uiDepth], resiLuma, uiPreCalcDistC, bSeparateLumaChroma DEBUG_STRING_PASS_INTO(sTest) );

  m_ppcRecoYuvTemp[uiDepth]->copyToPicComponent(COMPONENT_Y, rpcTempCU->getPic()->getPicYuvRec(), rpcTempCU->getAddr(), rpcTempCU->getZorderIdxInCU() );

  if (rpcBestCU->getPic()->getChromaFormat()!=CHROMA_400)
  {
    m_pcPredSearch->estIntraPredChromaQT( rpcTempCU, m_ppcOrigYuv[uiDepth], m_ppcPredYuvTemp[uiDepth], m_ppcResiYuvTemp[uiDepth], m_ppcRecoYuvTemp[uiDepth], resiLuma, uiPreCalcDistC DEBUG_STRING_PASS_INTO(sTest) );
  }

  m_pcEntropyCoder->resetBits();

  if ( rpcTempCU->getSlice()->getPPS()->getTransquantBypassEnableFlag())
  {
    m_pcEntropyCoder->encodeCUTransquantBypassFlag( rpcTempCU, 0,          true );
  }

  m_pcEntropyCoder->encodeSkipFlag ( rpcTempCU, 0,          true );
  m_pcEntropyCoder->encodePredMode( rpcTempCU, 0,          true );
  m_pcEntropyCoder->encodePartSize( rpcTempCU, 0, uiDepth, true );
  m_pcEntropyCoder->encodePredInfo( rpcTempCU, 0 );
  m_pcEntropyCoder->encodeIPCMInfo(rpcTempCU, 0, true );

  // Encode Coefficients
  Bool bCodeDQP = getdQPFlag();
  Bool codeChromaQpAdjFlag = getCodeChromaQpAdjFlag();
  m_pcEntropyCoder->encodeCoeff( rpcTempCU, 0, uiDepth, bCodeDQP, codeChromaQpAdjFlag );
  setCodeChromaQpAdjFlag( codeChromaQpAdjFlag );
  setdQPFlag( bCodeDQP );

  m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[uiDepth][CI_TEMP_BEST]);

  rpcTempCU->getTotalBits() = m_pcEntropyCoder->getNumberOfWrittenBits();
  rpcTempCU->getTotalBins() = ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
  rpcTempCU->getTotalCost() = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() );

  xCheckDQP( rpcTempCU );

  cost = rpcTempCU->getTotalCost();

  xCheckBestMode(rpcBestCU, rpcTempCU, uiDepth DEBUG_STRING_PASS_INTO(sDebug) DEBUG_STRING_PASS_INTO(sTest));
}


/** Check R-D costs for a CU with PCM mode.
 * \param rpcBestCU pointer to best mode CU data structure
 * \param rpcTempCU pointer to testing mode CU data structure
 * \returns Void
 *
 * \note Current PCM implementation encodes sample values in a lossless way. The distortion of PCM mode CUs are zero. PCM mode is selected if the best mode yields bits greater than that of PCM mode.
 */
Void TEncCu::xCheckIntraPCM( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU )
{
  UInt uiDepth = rpcTempCU->getDepth( 0 );

  rpcTempCU->setSkipFlagSubParts( false, 0, uiDepth );

  rpcTempCU->setIPCMFlag(0, true);
  rpcTempCU->setIPCMFlagSubParts (true, 0, rpcTempCU->getDepth(0));
  rpcTempCU->setPartSizeSubParts( SIZE_2Nx2N, 0, uiDepth );
  rpcTempCU->setPredModeSubParts( MODE_INTRA, 0, uiDepth );
  rpcTempCU->setTrIdxSubParts ( 0, 0, uiDepth );
  rpcTempCU->setChromaQpAdjSubParts( rpcTempCU->getCUTransquantBypass(0) ? 0 : m_ChromaQpAdjIdc, 0, uiDepth );

  m_pcPredSearch->IPCMSearch( rpcTempCU, m_ppcOrigYuv[uiDepth], m_ppcPredYuvTemp[uiDepth], m_ppcResiYuvTemp[uiDepth], m_ppcRecoYuvTemp[uiDepth]);

  m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[uiDepth][CI_CURR_BEST]);

  m_pcEntropyCoder->resetBits();

  if ( rpcTempCU->getSlice()->getPPS()->getTransquantBypassEnableFlag())
  {
    m_pcEntropyCoder->encodeCUTransquantBypassFlag( rpcTempCU, 0,          true );
  }

  m_pcEntropyCoder->encodeSkipFlag ( rpcTempCU, 0,          true );
  m_pcEntropyCoder->encodePredMode ( rpcTempCU, 0,          true );
  m_pcEntropyCoder->encodePartSize ( rpcTempCU, 0, uiDepth, true );
  m_pcEntropyCoder->encodeIPCMInfo ( rpcTempCU, 0, true );

  m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[uiDepth][CI_TEMP_BEST]);

  rpcTempCU->getTotalBits() = m_pcEntropyCoder->getNumberOfWrittenBits();
  rpcTempCU->getTotalBins() = ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
  rpcTempCU->getTotalCost() = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() );

  xCheckDQP( rpcTempCU );
  DEBUG_STRING_NEW(a)
  DEBUG_STRING_NEW(b)
  xCheckBestMode(rpcBestCU, rpcTempCU, uiDepth DEBUG_STRING_PASS_INTO(a) DEBUG_STRING_PASS_INTO(b));
}

/** check whether current try is the best with identifying the depth of current try
 * \param rpcBestCU
 * \param rpcTempCU
 * \returns Void
 */
Void TEncCu::xCheckBestMode( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, UInt uiDepth DEBUG_STRING_FN_DECLARE(sParent) DEBUG_STRING_FN_DECLARE(sTest) DEBUG_STRING_PASS_INTO(Bool bAddSizeInfo) )
{
  if( rpcTempCU->getTotalCost() < rpcBestCU->getTotalCost() )
  {
    TComYuv* pcYuv;
    // Change Information data
    TComDataCU* pcCU = rpcBestCU;
    rpcBestCU = rpcTempCU;
    rpcTempCU = pcCU;

    // Change Prediction data
    pcYuv = m_ppcPredYuvBest[uiDepth];
    m_ppcPredYuvBest[uiDepth] = m_ppcPredYuvTemp[uiDepth];
    m_ppcPredYuvTemp[uiDepth] = pcYuv;

    // Change Reconstruction data
    pcYuv = m_ppcRecoYuvBest[uiDepth];
    m_ppcRecoYuvBest[uiDepth] = m_ppcRecoYuvTemp[uiDepth];
    m_ppcRecoYuvTemp[uiDepth] = pcYuv;

    pcYuv = NULL;
    pcCU  = NULL;

    // store temp best CI for next CU coding
    m_pppcRDSbacCoder[uiDepth][CI_TEMP_BEST]->store(m_pppcRDSbacCoder[uiDepth][CI_NEXT_BEST]);


#ifdef DEBUG_STRING
    DEBUG_STRING_SWAP(sParent, sTest)
    const PredMode predMode=rpcBestCU->getPredictionMode(0);
    if ((DebugOptionList::DebugString_Structure.getInt()&DebugStringGetPredModeMask(predMode)) && bAddSizeInfo)
    {
      std::stringstream ss(stringstream::out);
      ss <<"###: " << (predMode==MODE_INTRA?"Intra   ":"Inter   ") << partSizeToString[rpcBestCU->getPartitionSize(0)] << " CU at " << rpcBestCU->getCUPelX() << ", " << rpcBestCU->getCUPelY() << " width=" << UInt(rpcBestCU->getWidth(0)) << std::endl;
      sParent+=ss.str();
    }
#endif
  }
}

Void TEncCu::xCheckDQP( TComDataCU* pcCU )
{
  UInt uiDepth = pcCU->getDepth( 0 );

  if( pcCU->getSlice()->getPPS()->getUseDQP() && (g_uiMaxCUWidth>>uiDepth) >= pcCU->getSlice()->getPPS()->getMinCuDQPSize() )
  {
    if ( pcCU->getQtRootCbf( 0) )
    {
#if !RDO_WITHOUT_DQP_BITS
      m_pcEntropyCoder->resetBits();
      m_pcEntropyCoder->encodeQP( pcCU, 0, false );
      pcCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // dQP bits
      pcCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
      pcCU->getTotalCost() = m_pcRdCost->calcRdCost( pcCU->getTotalBits(), pcCU->getTotalDistortion() );
#endif
    }
    else
    {
      pcCU->setQPSubParts( pcCU->getRefQP( 0 ), 0, uiDepth ); // set QP to default QP
    }
  }
}

Void TEncCu::xCopyAMVPInfo (AMVPInfo* pSrc, AMVPInfo* pDst)
{
  pDst->iN = pSrc->iN;
  for (Int i = 0; i < pSrc->iN; i++)
  {
    pDst->m_acMvCand[i] = pSrc->m_acMvCand[i];
  }
}
Void TEncCu::xCopyYuv2Pic(TComPic* rpcPic, UInt uiCUAddr, UInt uiAbsPartIdx, UInt uiDepth, UInt uiSrcDepth, TComDataCU* pcCU, UInt uiLPelX, UInt uiTPelY )
{
  UInt uiRPelX   = uiLPelX + (g_uiMaxCUWidth>>uiDepth)  - 1;
  UInt uiBPelY   = uiTPelY + (g_uiMaxCUHeight>>uiDepth) - 1;
  TComSlice * pcSlice = pcCU->getPic()->getSlice(pcCU->getPic()->getCurrSliceIdx());
  Bool bSliceStart = pcSlice->getSliceSegmentCurStartCUAddr() > rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx &&
    pcSlice->getSliceSegmentCurStartCUAddr() < rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+( pcCU->getPic()->getNumPartInCU() >> (uiDepth<<1) );
  Bool bSliceEnd   = pcSlice->getSliceSegmentCurEndCUAddr() > rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx &&
    pcSlice->getSliceSegmentCurEndCUAddr() < rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+( pcCU->getPic()->getNumPartInCU() >> (uiDepth<<1) );
  if(!bSliceEnd && !bSliceStart && ( uiRPelX < pcSlice->getSPS()->getPicWidthInLumaSamples() ) && ( uiBPelY < pcSlice->getSPS()->getPicHeightInLumaSamples() ) )
  {
    UInt uiAbsPartIdxInRaster = g_auiZscanToRaster[uiAbsPartIdx];
    UInt uiSrcBlkWidth = rpcPic->getNumPartInWidth() >> (uiSrcDepth);
    UInt uiBlkWidth    = rpcPic->getNumPartInWidth() >> (uiDepth);
    UInt uiPartIdxX = ( ( uiAbsPartIdxInRaster % rpcPic->getNumPartInWidth() ) % uiSrcBlkWidth) / uiBlkWidth;
    UInt uiPartIdxY = ( ( uiAbsPartIdxInRaster / rpcPic->getNumPartInWidth() ) % uiSrcBlkWidth) / uiBlkWidth;
    UInt uiPartIdx = uiPartIdxY * ( uiSrcBlkWidth / uiBlkWidth ) + uiPartIdxX;
    m_ppcRecoYuvBest[uiSrcDepth]->copyToPicYuv( rpcPic->getPicYuvRec (), uiCUAddr, uiAbsPartIdx, uiDepth - uiSrcDepth, uiPartIdx);

    m_ppcPredYuvBest[uiSrcDepth]->copyToPicYuv( rpcPic->getPicYuvPred (), uiCUAddr, uiAbsPartIdx, uiDepth - uiSrcDepth, uiPartIdx);
  }
  else
  {
    UInt uiQNumParts = ( pcCU->getPic()->getNumPartInCU() >> (uiDepth<<1) )>>2;

    for ( UInt uiPartUnitIdx = 0; uiPartUnitIdx < 4; uiPartUnitIdx++, uiAbsPartIdx+=uiQNumParts )
    {
      UInt uiSubCULPelX   = uiLPelX + ( g_uiMaxCUWidth >>(uiDepth+1) )*( uiPartUnitIdx &  1 );
      UInt uiSubCUTPelY   = uiTPelY + ( g_uiMaxCUHeight>>(uiDepth+1) )*( uiPartUnitIdx >> 1 );

      Bool bInSlice = rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+uiQNumParts > pcSlice->getSliceSegmentCurStartCUAddr() &&
        rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx < pcSlice->getSliceSegmentCurEndCUAddr();
      if(bInSlice&&( uiSubCULPelX < pcSlice->getSPS()->getPicWidthInLumaSamples() ) && ( uiSubCUTPelY < pcSlice->getSPS()->getPicHeightInLumaSamples() ) )
      {
        xCopyYuv2Pic( rpcPic, uiCUAddr, uiAbsPartIdx, uiDepth+1, uiSrcDepth, pcCU, uiSubCULPelX, uiSubCUTPelY );   // Copy Yuv data to picture Yuv
      }
    }
  }
}

Void TEncCu::xCopyYuv2Tmp( UInt uiPartUnitIdx, UInt uiNextDepth )
{
  UInt uiCurrDepth = uiNextDepth - 1;
  m_ppcRecoYuvBest[uiNextDepth]->copyToPartYuv( m_ppcRecoYuvTemp[uiCurrDepth], uiPartUnitIdx );
  m_ppcPredYuvBest[uiNextDepth]->copyToPartYuv( m_ppcPredYuvBest[uiCurrDepth], uiPartUnitIdx);
}

/** Function for filling the PCM buffer of a CU using its original sample array
 * \param pcCU pointer to current CU
 * \param pcOrgYuv pointer to original sample array
 * \returns Void
 */
Void TEncCu::xFillPCMBuffer     ( TComDataCU*& pCU, TComYuv* pOrgYuv )
{
  const ChromaFormat format = pCU->getPic()->getChromaFormat();
  const UInt numberValidComponents = getNumberValidComponents(format);
  for (UInt componentIndex = 0; componentIndex < numberValidComponents; componentIndex++)
  {
    const ComponentID component = ComponentID(componentIndex);

    const UInt width  = pCU->getWidth(0)  >> getComponentScaleX(component, format);
    const UInt height = pCU->getHeight(0) >> getComponentScaleY(component, format);

    Pel *source      = pOrgYuv->getAddr(component, 0, width);
    Pel *destination = pCU->getPCMSample(component);

    const UInt sourceStride = pOrgYuv->getStride(component);

    for (Int line = 0; line < height; line++)
    {
      for (Int column = 0; column < width; column++)
      {
        destination[column] = source[column];
      }

      source      += sourceStride;
      destination += width;
    }
  }
}

#if ADAPTIVE_QP_SELECTION
/** Collect ARL statistics from one block
  */
Int TEncCu::xTuCollectARLStats(TCoeff* rpcCoeff, TCoeff* rpcArlCoeff, Int NumCoeffInCU, Double* cSum, UInt* numSamples )
{
  for( Int n = 0; n < NumCoeffInCU; n++ )
  {
    TCoeff u = abs( rpcCoeff[ n ] );
    TCoeff absc = rpcArlCoeff[ n ];

    if( u != 0 )
    {
      if( u < LEVEL_RANGE )
      {
        cSum[ u ] += ( Double )absc;
        numSamples[ u ]++;
      }
      else
      {
        cSum[ LEVEL_RANGE ] += ( Double )absc - ( Double )( u << ARL_C_PRECISION );
        numSamples[ LEVEL_RANGE ]++;
      }
    }
  }

  return 0;
}

/** Collect ARL statistics from one LCU
 * \param pcCU
 */
Void TEncCu::xLcuCollectARLStats(TComDataCU* rpcCU )
{
  Double cSum[ LEVEL_RANGE + 1 ];     //: the sum of DCT coefficients corresponding to datatype and quantization output
  UInt numSamples[ LEVEL_RANGE + 1 ]; //: the number of coefficients corresponding to datatype and quantization output

  TCoeff* pCoeffY = rpcCU->getCoeff(COMPONENT_Y);
  TCoeff* pArlCoeffY = rpcCU->getArlCoeff(COMPONENT_Y);

  UInt uiMinCUWidth = g_uiMaxCUWidth >> g_uiMaxCUDepth;
  UInt uiMinNumCoeffInCU = 1 << uiMinCUWidth;

  memset( cSum, 0, sizeof( Double )*(LEVEL_RANGE+1) );
  memset( numSamples, 0, sizeof( UInt )*(LEVEL_RANGE+1) );

  // Collect stats to cSum[][] and numSamples[][]
  for(Int i = 0; i < rpcCU->getTotalNumPart(); i ++ )
  {
    UInt uiTrIdx = rpcCU->getTransformIdx(i);

    if(rpcCU->isInter(i) && rpcCU->getCbf( i, COMPONENT_Y, uiTrIdx ) )
    {
      xTuCollectARLStats(pCoeffY, pArlCoeffY, uiMinNumCoeffInCU, cSum, numSamples);
    }//Note that only InterY is processed. QP rounding is based on InterY data only.

    pCoeffY  += uiMinNumCoeffInCU;
    pArlCoeffY  += uiMinNumCoeffInCU;
  }

  for(Int u=1; u<LEVEL_RANGE;u++)
  {
    m_pcTrQuant->getSliceSumC()[u] += cSum[ u ] ;
    m_pcTrQuant->getSliceNSamples()[u] += numSamples[ u ] ;
  }
  m_pcTrQuant->getSliceSumC()[LEVEL_RANGE] += cSum[ LEVEL_RANGE ] ;
  m_pcTrQuant->getSliceNSamples()[LEVEL_RANGE] += numSamples[ LEVEL_RANGE ] ;
}
#endif
//! \}
