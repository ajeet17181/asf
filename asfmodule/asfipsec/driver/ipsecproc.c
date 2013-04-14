/**************************************************************************
 * Copyright 2010-2012, Freescale Semiconductor, Inc. All rights reserved.
 ***************************************************************************/
/*
 * File:	ipsecproc.c
 * Description: ASF IPSEC proc interface implementation
 * Authors:	Hemant Agrawal <hemant@freescale.com>
 *
 */
 /* History
 *  Version	Date		Author		Change Description
 *  0.1		12/10/2010    Hemant Agrawal	Initial Development
 *
*/
/***************************************************************************/

#include <linux/version.h>
#include <linux/skbuff.h>
#include "../../asfffp/driver/asf.h"
#include "../../asfffp/driver/asfparry.h"
#include "../../asfffp/driver/asftmr.h"
#include "../../asfffp/driver/gplcode.h"
#include "ipsfpapi.h"
#include "ipsecfp.h"
#include "ipseccmn.h"
#include <linux/proc_fs.h>
/*
 * Implement following proc
 *	/proc/asf/ipsec/flows
 *	/proc/asf/ipsec/stats
 */

enum {
	SECFP_PROC_COMMAND = 1,
	SECFP_MAX_TUNNELS,
	SECFP_MAX_VSGS,
	SECFP_MAX_SPD,
	SECFP_MAX_SA,
	SECFP_L2BLOB_REFRESH_NPKTS,
	SECFP_L2BLOB_REFRESH_INTERVAL
} ;
struct algo_info {
	const char *alg_name;
	int alg_type;
};

#define IPSEC_PROC_MAX_ALGO 8

static const struct algo_info algo_types[2][IPSEC_PROC_MAX_ALGO] = {
	{
		{"cbc(aes)", SECFP_AES},
		{"cbc(des3_ede)", SECFP_3DES},
		{"cbc(des)", SECFP_DES},
		{"null", SECFP_ESP_NULL},
		{"aes-ctr)", SECFP_AESCTR},
		{NULL, -1}
	},
	{
		{"hmac(sha1)", SECFP_HMAC_SHA1},
		{"hmac(sha256)", SECFP_HMAC_SHA256},
		{"hmac(sha384)", SECFP_HMAC_SHA384},
		{"hmac(sha512)", SECFP_HMAC_SHA512},
		{"hmac(md5)", SECFP_HMAC_MD5},
		{"aes-xcbc-mac)", SECFP_HMAC_AES_XCBC_MAC},
		{"null", SECFP_HMAC_NULL},
		{NULL, -1}
	}
};

const char *algo_getname(int type, int algo)
{
	int i;
	for (i = 0; i < IPSEC_PROC_MAX_ALGO; i++) {
		if (algo == algo_types[type][i].alg_type)
			return algo_types[type][i].alg_name;
	}
	return "NULL";
}


static struct ctl_table secfp_proc_table[] = {
	{
		.procname       = "ulMaxTunnels_g",
		.data	   = &ulMaxTunnels_g,
		.maxlen	 = sizeof(int),
		.mode	   = 0444,
		.proc_handler   = proc_dointvec,
	} ,
	{
		.procname       = "ulMaxVSGs_g",
		.data	   = &ulMaxVSGs_g,
		.maxlen	 = sizeof(int),
		.mode	   = 0444,
		.proc_handler   = proc_dointvec,
	} ,
	{
		.procname       = "ulMaxSPDContainers_g",
		.data	   = &ulMaxSPDContainers_g,
		.maxlen	 = sizeof(int),
		.mode	   = 0444,
		.proc_handler   = proc_dointvec,
	} ,
	{
		.procname	= "ulMaxSupportedIPSecSAs_g",
		.data	   = &ulMaxSupportedIPSecSAs_g,
		.maxlen  = sizeof(int),
		.mode	   = 0444,
		.proc_handler	= proc_dointvec,
	} ,
	{
		.procname       = "ulL2BlobRefreshPktCnt_g",
		.data	   = &ulL2BlobRefreshPktCnt_g,
		.maxlen	 = sizeof(int),
		.mode	   = 0644,
		.proc_handler   = proc_dointvec,
	} ,
	{
		.procname       = "ulL2BlobRefreshTimeInSec_g",
		.data	   = &ulL2BlobRefreshTimeInSec_g,
		.maxlen	 = sizeof(int),
		.mode	   = 0644,
		.proc_handler   = proc_dointvec,
	} ,
	{}
} ;

static struct ctl_table secfp_proc_root_table[] = {
	{
		.procname       = "asfipsec",
		.mode	   = 0555,
		.child	  = secfp_proc_table,
	} ,
	{}
} ;

static struct ctl_table_header *secfp_proc_header;


static struct proc_dir_entry *secfp_dir;
#define SECFP_PROC_GLOBAL_STATS_NAME	"global_stats"
#define SECFP_PROC_RESET_STATS_NAME	"reset_stats"
#define SECFP_PROC_GLOBAL_ERROR_NAME	"global_error"
#define SECFP_PROC_OUT_SPD		"out_spd"
#define SECFP_PROC_IN_SPD		"in_spd"
#define SECFP_PROC_OUT_SA		"out_sa"
#define SECFP_PROC_IN_SA		"in_sa"

#define GSTATS_SUM(a) (total.ul##a += gstats->ul##a)
#define GSTATS_TOTAL(a) (unsigned long) total.ul##a
static int display_secfp_proc_global_stats(char *page, char **start,
					 off_t off, int count,
					 int *eof, void *data)
{
	AsfIPSecPPGlobalStats_t total;
	int cpu;

	memset(&total, 0, sizeof(total));

	for_each_online_cpu(cpu) {
		AsfIPSecPPGlobalStats_t *gstats;
		gstats = asfPerCpuPtr(pIPSecPPGlobalStats_g, cpu);
		GSTATS_SUM(TotInRecvPkts);
		GSTATS_SUM(TotInProcPkts);
		GSTATS_SUM(TotOutRecvPkts);
		GSTATS_SUM(TotOutProcPkts);
		GSTATS_SUM(TotInRecvSecPkts);
		GSTATS_SUM(TotInProcSecPkts);
		GSTATS_SUM(TotOutRecvPktsSecApply);
		GSTATS_SUM(TotOutPktsSecAppled);
		printk(KERN_INFO"\n cpu =%d Encrypted Pkts = %u Normal Pkts = %u",
			cpu, gstats->ulTotInRecvPkts, gstats->ulTotOutRecvPkts);
	}

	printk(KERN_INFO"\n    InRcv %lu\tInProc %lu\tOutRcv %lu OutProc %lu\n",
		GSTATS_TOTAL(TotInRecvPkts), GSTATS_TOTAL(TotInProcPkts),
		GSTATS_TOTAL(TotOutRecvPkts), GSTATS_TOTAL(TotOutProcPkts));

	printk(KERN_INFO"\nSEC-InRcv %lu\tInProc %lu\tOutRcv %lu OutProc %lu\n",
		GSTATS_TOTAL(TotInRecvSecPkts),
		GSTATS_TOTAL(TotInProcSecPkts),
		GSTATS_TOTAL(TotOutRecvPktsSecApply),
		GSTATS_TOTAL(TotOutPktsSecAppled));

	return 0;
}

static int reset_secfp_proc_global_stats(char *page, char **start,
					off_t off, int count,
					int *eof, void *data)
{
	ASFIPSec4GlobalPPStats_t Outparams;

	ASFIPSecGlobalQueryStats(&Outparams, ASF_TRUE);
	memset(&GlobalErrors, 0, sizeof(ASFIPSecGlobalErrorCounters_t));

	printk(KERN_INFO"\n    InRcv %u \t InProc %u \tOutRcv %u OutProc %u\n",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT1],
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT2],
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT3],
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT4]);

	printk(KERN_INFO"\nSEC-InRcv %u \t InProc %u \tOutRcv %u OutProc %u\n",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT5],
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT6],
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT7],
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT8]);

	printk(KERN_INFO"Resetting IPSEC Global Stats\n");

	return 0;
}

#define GLBERR_DISP(a) printk(KERN_INFO" " #a " = %u\n", total->ul##a)
static int display_secfp_proc_global_errors(char *page, char **start,
					off_t off, int count,
					int *eof, void *data)
{
	ASFIPSecGlobalErrorCounters_t *total;
	ASFIPSec4GlobalPPStats_t Outparams;
	total = &GlobalErrors;

	GLBERR_DISP(InvalidVSGId);
	GLBERR_DISP(InvalidTunnelId);
	GLBERR_DISP(InvalidMagicNumber);
	GLBERR_DISP(InvalidInSPDContainerId);
	GLBERR_DISP(InvalidOutSPDContainerId);
	GLBERR_DISP(InSPDContainerAlreadyPresent);
	GLBERR_DISP(OutSPDContainerAlreadyPresent);
	GLBERR_DISP(ResourceNotAvailable);
	GLBERR_DISP(TunnelIdNotInUse);
	GLBERR_DISP(TunnelIfaceFull);
	GLBERR_DISP(OutSPDContainersFull);
	GLBERR_DISP(InSPDContainersFull);
	GLBERR_DISP(SPDOutContainerNotFound);
	GLBERR_DISP(SPDInContainerNotFound);
	GLBERR_DISP(OutDuplicateSA);
	GLBERR_DISP(InDuplicateSA);
	GLBERR_DISP(InvalidAuthEncAlgo);
	GLBERR_DISP(OutSAFull);
	GLBERR_DISP(OutSANotFound);
	GLBERR_DISP(InSAFull);
	GLBERR_DISP(InSANotFound);
	GLBERR_DISP(InSASPDContainerMisMatch);
	GLBERR_DISP(OutSASPDContainerMisMatch);

	ASFIPSecGlobalQueryStats(&Outparams, ASF_FALSE);
	printk(KERN_INFO" \nERRORS:");
	printk(KERN_INFO"%u (Does not enough tail room to continue)",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT9]);
	printk(KERN_INFO"%u (No of packets Invalid ESP)",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT10]);
	printk(KERN_INFO"%u (Decrypted Protocol != IPV4)",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT11]);
	printk(KERN_INFO"%u (Invalid Pad length)",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT12]);
	printk(KERN_INFO"%u (Submission to SEC failed)",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT13]);
	printk(KERN_INFO"%u (Invalid sequence number )",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT14]);
	printk(KERN_INFO"%u (Anti-replay window check failed )",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT15]);
	printk(KERN_INFO"%u (Replay packet )",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT16]);
	printk(KERN_INFO"%u (ICV Comp Failed )",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT17]);
	printk(KERN_INFO"%u (Crypto Operation Failed )",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT18]);
	printk(KERN_INFO"%u (Anti Replay window -- Drop the packet )",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT19]);
	printk(KERN_INFO"%u (Verification of SA Selectross Failed )",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT20]);
	printk(KERN_INFO"%u (Packet size is > Path MTU and"\
		"fragment bit set in SA or packet )",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT21]);
	printk(KERN_INFO"%u (Fragmentation Failed )",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT22]);
	printk(KERN_INFO"%u (IN SA Not Found )",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT23]);
	printk(KERN_INFO"%u (OUT SA Not Found )\n",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT24]);
	printk(KERN_INFO"%u (L2blob Not Found )\n",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT25]);
	printk(KERN_INFO"%u (Desc Alloc Error )\n",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT26]);
	printk(KERN_INFO"%u (SA Expired )\n",
		Outparams.IPSec4GblPPStat[ASF_IPSEC_PP_GBL_CNT27]);
	return 0;
}

static void print_SPDPolPPStats(AsfSPDPolPPStats_t PPStats)
{
	return;
}

static int display_secfp_proc_out_spd(char *page, char **start,
				off_t off, int count,
				int *eof, void *data)
{
	ASF_uint32_t ulVSGId = 0;
	ASF_uint32_t ulTunnelId = 0;
	struct SPDCILinkNode_s *pCINode;
	SPDOutContainer_t *pOutContainer = NULL;
	SPDOutSALinkNode_t *pOutSALinkNode;

	int bVal = in_softirq();

	if (!bVal)
		local_bh_disable();

	if (secFP_TunnelIfaces[ulVSGId][ulTunnelId].bInUse == 0) {
		printk(KERN_INFO"Tunnel Interface is not in use"\
			".TunnelId=%u, VSGId=%u\n",
			ulTunnelId, ulVSGId);
		if (!bVal)
			local_bh_enable();
		return ASF_IPSEC_TUNNEL_NOT_FOUND;
	}
	printk(KERN_INFO"\nVSGID= %d TUNNELID= %d, MAGIC NUM = %d\n",
		ulVSGId, ulTunnelId,
		secFP_TunnelIfaces[ulVSGId][ulTunnelId].ulTunnelMagicNumber);

	pCINode = secFP_TunnelIfaces[ulVSGId][ulTunnelId].pSPDCIOutList;
	for (; pCINode != NULL; pCINode = pCINode->pNext) {

		pOutContainer = (SPDOutContainer_t *)(ptrIArray_getData(
					&(secfp_OutDB),
					pCINode->ulIndex));
		if (!pOutContainer)
			continue;
		printk(KERN_INFO"=========OUT Policy==================\n");
		printk(KERN_INFO"Id=%d, Proto 0x%x, Dscp 0x%x"\
			"Flags:Udp(%d) RED(%d),ESN(%d),DSCP(%d),DF(%d)\n",
		pCINode->ulIndex,
		pOutContainer->SPDParams.ucProto,
		pOutContainer->SPDParams.ucDscp,
		pOutContainer->SPDParams.bUdpEncap,
		pOutContainer->SPDParams.bRedSideFrag,
		pOutContainer->SPDParams.bESN,
		pOutContainer->SPDParams.bCopyDscp,
		pOutContainer->SPDParams.handleDf);

		print_SPDPolPPStats(pOutContainer->PPStats);

		printk(KERN_INFO"List SA IDs:");
		for (pOutSALinkNode = pOutContainer->SAHolder.pSAList;
			pOutSALinkNode != NULL;
			pOutSALinkNode = pOutSALinkNode->pNext) {
			printk(KERN_INFO" %d ", pOutSALinkNode->ulSAIndex);
			if (pOutSALinkNode->ulSAIndex % 10)
				printk(KERN_INFO"\n\t");
		}
		printk(KERN_INFO"\n");
	}
	if (!bVal)
		local_bh_enable();
	return 0;
}

static int display_secfp_proc_in_spd(char *page, char **start,
				off_t off, int count,
				int *eof, void *data)
{
	int ulSAIndex;
	ASF_uint32_t ulVSGId = 0;
	ASF_uint32_t ulTunnelId = 0;
	struct SPDCILinkNode_s *pCINode;
	SPDInContainer_t *pInContainer = NULL;
	SPDInSPIValLinkNode_t *pSPILinkNode;

	int bVal = in_softirq();

	if (!bVal)
		local_bh_disable();

	if (secFP_TunnelIfaces[ulVSGId][ulTunnelId].bInUse == 0) {
		printk(KERN_INFO"\nTunnel Interface is not in use"\
			".TunnelId=%u, VSGId=%u\n",
			ulTunnelId, ulVSGId);
		if (!bVal)
			local_bh_enable();
		return ASF_IPSEC_TUNNEL_NOT_FOUND;
	}
	printk(KERN_INFO"\nVSGID= %d TUNNELID= %d, MAGIC NUM = %d",
		ulVSGId, ulTunnelId,
		secFP_TunnelIfaces[ulVSGId][ulTunnelId].ulTunnelMagicNumber);

	pCINode = secFP_TunnelIfaces[ulVSGId][ulTunnelId].pSPDCIInList;
	for (; pCINode != NULL; pCINode = pCINode->pNext) {

		pInContainer = (SPDInContainer_t *)(ptrIArray_getData(
					&(secfp_InDB),
					pCINode->ulIndex));
		if (!pInContainer)
			continue;
		printk(KERN_INFO"=========IN Policy==================\n");
		printk(KERN_INFO"Id=%d, Proto 0x%x, Dscp 0x%x "\
			"Flags:Udp(%d) ESN(%d),DSCP(%d),ECN(%d)\n",
		pCINode->ulIndex,
		pInContainer->SPDParams.ucProto,
		pInContainer->SPDParams.ucDscp,
		pInContainer->SPDParams.bUdpEncap,
		pInContainer->SPDParams.bESN,
		pInContainer->SPDParams.bCopyDscp,
		pInContainer->SPDParams.bCopyEcn);

		print_SPDPolPPStats(pInContainer->PPStats);

		printk(KERN_INFO"List IN SA -SPI Val:");

		for (pSPILinkNode = pInContainer->pSPIValList, ulSAIndex = 0;
			pSPILinkNode != NULL;
			pSPILinkNode = pSPILinkNode->pNext, ulSAIndex++) {

			printk(KERN_INFO"0x%x ", pSPILinkNode->ulSPIVal);
			if (ulSAIndex % 10)
				printk(KERN_INFO"\n");
		}
		printk(KERN_INFO"\n");
	}
	if (!bVal)
		local_bh_enable();
	return 0;
}

static void print_SAParams(SAParams_t *SAParams)
{
	printk(KERN_INFO"CId = %d TunnelInfo src = 0x%x,dst = 0x%x SPI=0x%x",
		SAParams->ulCId,
		SAParams->tunnelInfo.addr.iphv4.saddr,
		SAParams->tunnelInfo.addr.iphv4.daddr,
		SAParams->ulSPI);

	printk(KERN_INFO"\nProtocol = 0x%x, Dscp = 0x%x,"\
		"AuthAlgo =%s(%d)(Len=%d), CipherAlgo = %s(%d) (Len=%d) ",
		SAParams->ucProtocol, SAParams->ucDscp,
		algo_getname(1, SAParams->ucAuthAlgo),
		SAParams->ucAuthAlgo, SAParams->AuthKeyLen,
		algo_getname(0, SAParams->ucCipherAlgo),
		SAParams->ucCipherAlgo, SAParams->EncKeyLen);

	printk(KERN_INFO"AntiReplay = %d, UDPEncap(NAT-T) = %d\n",
		SAParams->bDoAntiReplayCheck,
		SAParams->bDoUDPEncapsulationForNATTraversal);

	printk(KERN_INFO "LifeKBytes Soft = %lu - Hard = %lu:",
		SAParams->softKbyteLimit,
		SAParams->hardKbyteLimit);

	printk(KERN_INFO "LifePacket Soft = %lu - Hard = %lu:",
		SAParams->softPacketLimit,
		SAParams->hardPacketLimit);
}

static int display_secfp_proc_out_sa(char *page, char **start,
				off_t off, int count,
				int *eof, void *data)
{
	ASF_uint32_t ulVSGId = 0;
	ASF_uint32_t ulTunnelId = 0;
	struct SPDCILinkNode_s *pCINode;
	SPDOutContainer_t *pOutContainer = NULL;
	SPDOutSALinkNode_t *pOutSALinkNode;
	outSA_t *pOutSA = NULL;

	int bVal = in_softirq();

	if (!bVal)
		local_bh_disable();

	if (secFP_TunnelIfaces[ulVSGId][ulTunnelId].bInUse == 0) {
		printk(KERN_INFO"Tunnel Interface is not in use"\
			".TunnelId=%u, VSGId=%u\n",
			ulTunnelId, ulVSGId);
		if (!bVal)
			local_bh_enable();
		return ASF_IPSEC_TUNNEL_NOT_FOUND;
	}
	printk(KERN_INFO"\nVSGID= %d TUNNELID= %d, MAGIC NUM = %d\n",
		ulVSGId, ulTunnelId,
		secFP_TunnelIfaces[ulVSGId][ulTunnelId].ulTunnelMagicNumber);

	pCINode = secFP_TunnelIfaces[ulVSGId][ulTunnelId].pSPDCIOutList;
	for (; pCINode != NULL; pCINode = pCINode->pNext) {

		pOutContainer = (SPDOutContainer_t *)(ptrIArray_getData(
					&(secfp_OutDB),
					pCINode->ulIndex));
		if (!pOutContainer)
			continue;
		printk(KERN_INFO"=========OUT Policy==================\n");
		printk(KERN_INFO"Id=%d, Proto %d, Dscp %d "\
			"Flags:Udp(%d) RED(%d),ESN(%d),DSCP(%d),DF(%d)\n",
		pCINode->ulIndex,
		pOutContainer->SPDParams.ucProto,
		pOutContainer->SPDParams.ucDscp,
		pOutContainer->SPDParams.bUdpEncap,
		pOutContainer->SPDParams.bRedSideFrag,
		pOutContainer->SPDParams.bESN,
		pOutContainer->SPDParams.bCopyDscp,
		pOutContainer->SPDParams.handleDf);

		print_SPDPolPPStats(pOutContainer->PPStats);
		printk(KERN_INFO"--------------SA_LIST--------------------");
		for (pOutSALinkNode = pOutContainer->SAHolder.pSAList;
			pOutSALinkNode != NULL;
			pOutSALinkNode = pOutSALinkNode->pNext) {
			printk(KERN_INFO"\nSA-ID= %d ", pOutSALinkNode->ulSAIndex);
			pOutSA =
				(outSA_t *) ptrIArray_getData(&secFP_OutSATable,
					pOutSALinkNode->ulSAIndex);
			if (pOutSA) {
				ASFSAStats_t outParams = {0, 0};
				ASFIPSecGetSAQueryParams_t inParams;

				print_SAParams(&pOutSA->SAParams);

				inParams.ulVSGId = ulVSGId;
				inParams.ulTunnelId = ulTunnelId;
				inParams.ulSPDContainerIndex = pCINode->ulIndex;
				inParams.ulSPI = pOutSA->SAParams.ulSPI;
				inParams.gwAddr.bIPv4OrIPv6 = pOutSA->SAParams.tunnelInfo.bIPv4OrIPv6;
				if (pOutSA->SAParams.tunnelInfo.bIPv4OrIPv6)
					memcpy(inParams.gwAddr.ipv6addr, pOutSA->SAParams.tunnelInfo.addr.iphv6.daddr, 16);
				else
					inParams.gwAddr.ipv4addr =
						pOutSA->SAParams.tunnelInfo.addr.iphv4.daddr;

				inParams.ucProtocol =
						pOutSA->SAParams.ucProtocol;
				inParams.bDir = SECFP_OUT;
				ASFIPSecSAQueryStats(&inParams, &outParams);
				printk(KERN_INFO"Stats:ulBytes=%llu, ulPkts=%llu",
					outParams.ulBytes, outParams.ulPkts);

				printk(KERN_INFO"L2BlobLen = %d, Magic = %d\n",
					pOutSA->ulL2BlobLen,
				pOutSA->l2blobConfig.ulL2blobMagicNumber);
#ifdef ASF_QMAN_IPSEC
				printk(KERN_INFO"SecFQ=%d, RecvFQ=%d\n",
					pOutSA->ctx.SecFq->qman_fq.fqid,
					pOutSA->ctx.RecvFq->qman_fq.fqid);
#endif
			}
		}
		printk(KERN_INFO"\n");
	}
	if (!bVal)
		local_bh_enable();

	return 0;
}
static int display_secfp_proc_in_sa(char *page, char **start,
				off_t off, int count,
				int *eof, void *data)
{
	int ulSAIndex;
	ASF_uint32_t ulVSGId = 0;
	ASF_uint32_t ulTunnelId = 0;
	struct SPDCILinkNode_s *pCINode;
	SPDInContainer_t *pInContainer = NULL;
	SPDInSPIValLinkNode_t *pSPILinkNode;
	inSA_t  *pInSA =  NULL;
	unsigned int ulHashVal;

	int bVal = in_softirq();

	if (!bVal)
		local_bh_disable();

	if (secFP_TunnelIfaces[ulVSGId][ulTunnelId].bInUse == 0) {
		printk(KERN_INFO"Tunnel Interface is not in use"\
			".TunnelId=%u, VSGId=%u\n",
			ulTunnelId, ulVSGId);
		if (!bVal)
			local_bh_enable();
		return ASF_IPSEC_TUNNEL_NOT_FOUND;
	}
	printk(KERN_INFO"\nVSGID= %d TUNNELID= %d, MAGIC NUM = %d\n",
		ulVSGId, ulTunnelId,
		secFP_TunnelIfaces[ulVSGId][ulTunnelId].ulTunnelMagicNumber);

	pCINode = secFP_TunnelIfaces[ulVSGId][ulTunnelId].pSPDCIInList;
	for (; pCINode != NULL; pCINode = pCINode->pNext) {

		pInContainer = (SPDInContainer_t *)(ptrIArray_getData(
					&(secfp_InDB),
					pCINode->ulIndex));
		if (!pInContainer)
			continue;

		printk(KERN_INFO"=========IN Policy==================\n");
		printk(KERN_INFO"Id=%d, Proto %d, Dscp %d "\
			"Flags:Udp(%d) ESN(%d),DSCP(%d),ECN(%d)\n",
		pCINode->ulIndex,
		pInContainer->SPDParams.ucProto,
		pInContainer->SPDParams.ucDscp,
		pInContainer->SPDParams.bUdpEncap,
		pInContainer->SPDParams.bESN,
		pInContainer->SPDParams.bCopyDscp,
		pInContainer->SPDParams.bCopyEcn);

		print_SPDPolPPStats(pInContainer->PPStats);
		printk(KERN_INFO"--------------SA_LIST--------------------");
		for (pSPILinkNode = pInContainer->pSPIValList, ulSAIndex = 0;
			pSPILinkNode != NULL;
			pSPILinkNode = pSPILinkNode->pNext, ulSAIndex++) {
			printk(KERN_INFO"\nSPI = 0x%x", pSPILinkNode->ulSPIVal);
			ulHashVal = secfp_compute_hash(pSPILinkNode->ulSPIVal);
			for (pInSA = secFP_SPIHashTable[ulHashVal].pHeadSA;
				pInSA != NULL; pInSA = pInSA->pNext) {

				ASFSAStats_t outParams = {0, 0};
				ASFIPSecGetSAQueryParams_t inParams;

				printk(KERN_INFO"SpdContId =%d",
					pInSA->ulSPDInContainerIndex);
				print_SAParams(&pInSA->SAParams);

				inParams.ulVSGId = ulVSGId;
				inParams.ulTunnelId = ulTunnelId;
				inParams.ulSPDContainerIndex = pCINode->ulIndex;
				inParams.ulSPI = pInSA->SAParams.ulSPI;
				inParams.gwAddr.bIPv4OrIPv6 = pInSA->SAParams.tunnelInfo.bIPv4OrIPv6;
				if (pInSA->SAParams.tunnelInfo.bIPv4OrIPv6)
					memcpy(inParams.gwAddr.ipv6addr, pInSA->SAParams.tunnelInfo.addr.iphv6.daddr, 16);
				else
					inParams.gwAddr.ipv4addr =
						pInSA->SAParams.tunnelInfo.addr.iphv4.daddr;

				inParams.ucProtocol =
					pInSA->SAParams.ucProtocol;
				inParams.bDir = SECFP_IN;
				ASFIPSecSAQueryStats(&inParams, &outParams);
				printk(KERN_INFO"Stats:ulBytes=%llu,ulPkts= %llu",
					outParams.ulBytes, outParams.ulPkts);
#ifdef ASF_QMAN_IPSEC
				printk(KERN_INFO"SecFQ=%d, RecvFQ=%d\n",
					pInSA->ctx.SecFq->qman_fq.fqid,
					pInSA->ctx.RecvFq->qman_fq.fqid);
#endif
			}
		}
		printk(KERN_INFO"\n");
	}
	if (!bVal)
		local_bh_enable();
	return 0;

}

int secfp_register_proc(void)
{

	/* register sysctl tree */
	secfp_proc_header = register_sysctl_table(secfp_proc_root_table);
	if (!secfp_proc_header)
		return -ENOMEM;

	/* register other under /proc/asfipsec */
	secfp_dir =  proc_mkdir("asfipsec", NULL);

	if (secfp_dir == NULL)
		return -ENOMEM;

	create_proc_read_entry(
					SECFP_PROC_GLOBAL_STATS_NAME,
					0444, secfp_dir,
					display_secfp_proc_global_stats,
					NULL);

	create_proc_read_entry(
					SECFP_PROC_RESET_STATS_NAME,
					0444, secfp_dir,
					reset_secfp_proc_global_stats,
					NULL);

	create_proc_read_entry(
					SECFP_PROC_GLOBAL_ERROR_NAME,
					0444, secfp_dir,
					display_secfp_proc_global_errors,
					NULL);

	create_proc_read_entry(
					SECFP_PROC_OUT_SPD,
					0444, secfp_dir,
					display_secfp_proc_out_spd,
					NULL);

	create_proc_read_entry(
					SECFP_PROC_IN_SPD,
					0444, secfp_dir,
					display_secfp_proc_in_spd,
					NULL);

	create_proc_read_entry(
					SECFP_PROC_OUT_SA,
					0444, secfp_dir,
					display_secfp_proc_out_sa,
					NULL);

	create_proc_read_entry(
					SECFP_PROC_IN_SA,
					0444, secfp_dir,
					display_secfp_proc_in_sa,
					NULL);

	return 0;
}


int secfp_unregister_proc(void)
{
	if (secfp_proc_header)
		unregister_sysctl_table(secfp_proc_header);

	remove_proc_entry(SECFP_PROC_GLOBAL_STATS_NAME, secfp_dir);
	remove_proc_entry(SECFP_PROC_RESET_STATS_NAME, secfp_dir);
	remove_proc_entry(SECFP_PROC_GLOBAL_ERROR_NAME, secfp_dir);
	remove_proc_entry(SECFP_PROC_OUT_SPD, secfp_dir);
	remove_proc_entry(SECFP_PROC_IN_SPD, secfp_dir);
	remove_proc_entry(SECFP_PROC_OUT_SA, secfp_dir);
	remove_proc_entry(SECFP_PROC_IN_SA, secfp_dir);
	remove_proc_entry("asfipsec", NULL);

	return 0;
}
