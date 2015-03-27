// dumpCFindResponse.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */
#include "dcmtk/dcmnet/dfindscu.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/ofstd/ofconapp.h"
#include "dcmtk/dcmdata/dcdict.h"
#include "dcmtk/ofstd/ofstdinc.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcdicent.h"
#include "dcmtk/dcmdata/dcdict.h"
#include "dcmtk/dcmdata/dcpath.h"
#include "dcmtk/ofstd/ofconapp.h"
#include "dcmtk/dcmdata/dcpxitem.h"
#include "dcmtk/dcmdata//dctk.h"
#include "ZSCFindCallback.h"
#include "ZSUtilities.h"

//--------------------Ӳ����ȫ�ֱ���----------------------------------
UINT32 maxReceivePDULength=ASC_DEFAULTMAXPDU;
const char* ourTitle="ZSSURE";
const char* peerTitle="OFFIS";
const char* peer="127.0.0.1";
unsigned int port=2234;
const char* abstractSyntax=UID_FINDModalityWorklistInformationModel;
const char* transferSyntaxs[]={
	UID_LittleEndianExplicitTransferSyntax,
	UID_BigEndianExplicitTransferSyntax,
	UID_LittleEndianImplicitTransferSyntax
};
int transferSyntaxNum=3;
//-----------------------zssure:end------------------------------------
void InsertQueryItems(DcmDataset*& dataset,const char* patientName=NULL,const char* patientID=NULL)
{
	if(patientName==NULL && patientID==NULL)
		return;
	if(patientName!=NULL)
		dataset->putAndInsertString(DCM_PatientName,patientName);
	else
	{
		dataset->putAndInsertString(DCM_PatientName,"");

	}
	if(patientID!=NULL)
		dataset->putAndInsertString(DCM_PatientID,patientID);
	else
	{
		dataset->putAndInsertString(DCM_PatientID,"");

	}
	//�̶���ѯ�����Բ���������ֻ����������UID��ѯ
	dataset->putAndInsertString(DCM_StudyInstanceUID,"");
	dataset->putAndInsertString(DCM_SeriesInstanceUID,"");
	dataset->putAndInsertString(DCM_SOPInstanceUID,"");
}
int _tmain(int argc, _TCHAR* argv[])
{
	//1)��ʼ�����绷��
	WSAData winSockData;
	/* we need at least version 1.1 */
	WORD winSockVersionNeeded = MAKEWORD( 1, 1 );
	WSAStartup(winSockVersionNeeded, &winSockData);
	//2��DCMTK�������
	if(!dcmDataDict.isDictionaryLoaded())
	{
		printf("No data dictionary loaded, check environment variable\n");
	}
	//3�������ASC��ʼ��
	T_ASC_Network* cfindNetwork=NULL;
	int timeout=50;
	OFCondition cond=ASC_initializeNetwork(NET_REQUESTOR,0,timeout,&cfindNetwork);
	if(cond.bad())
	{
		printf("DICOM �ײ������ʼ��ʧ��\n");
		return -1;
	}
	//4�������ײ����ӣ���TCP��
	T_ASC_Association* assoc=NULL;
	T_ASC_Parameters* params=NULL;
	DIC_NODENAME localHost;
	DIC_NODENAME peerHost;
	OFString temp_str;
	cond=ASC_createAssociationParameters(&params,maxReceivePDULength);
	if(cond.bad())
	{
		printf("DCMTK��������ʧ��\n");
		return -2;
	}
	//5������DICOM������ԣ�Presentation Context
	ASC_setAPTitles(params, ourTitle, peerTitle, NULL);

	cond = ASC_setTransportLayerType(params, false);
	if (cond.bad()) return -3;


	gethostname(localHost, sizeof(localHost) - 1);
	sprintf(peerHost, "%s:%d", peer, OFstatic_cast(int, port));
	ASC_setPresentationAddresses(params, localHost, peerHost);

	cond=ASC_addPresentationContext(params,1,abstractSyntax,transferSyntaxs,transferSyntaxNum);
	if(cond.bad())
		return -4;
	//6��������������
	cond=ASC_requestAssociation(cfindNetwork,params,&assoc);
	if (cond.bad()) {
		if (cond == DUL_ASSOCIATIONREJECTED) {
			T_ASC_RejectParameters rej;
			ASC_getRejectParameters(params, &rej);

			DCMNET_ERROR("Association Rejected:" << OFendl << ASC_printRejectParameters(temp_str, &rej));
			return -5;
		} else {
			DCMNET_ERROR("Association Request Failed: " << DimseCondition::dump(temp_str, cond));
			return -6;
		}
	}
	//7���б𷵻ؽ��
	//7.1)���Ӽ���׶Σ���֤Presentation Context
	if(ASC_countAcceptedPresentationContexts(params)==0)
	{
		printf("No acceptable Presentation Contexts\n");
		return -7;
	}
	T_ASC_PresentationContextID presID;
	T_DIMSE_C_FindRQ req;
	T_DIMSE_C_FindRSP rsp;
	DcmFileFormat dcmff;
	
	presID=ASC_findAcceptedPresentationContextID(assoc,abstractSyntax);
	if(presID==0)
	{
		printf("No presentation context\n");
		return -8;
	}
	//8������C-FIND����
	//8.1��׼��C-FIND-RQ message
	bzero(OFreinterpret_cast(char*,&req),sizeof(req));//�ڴ��ʼ��Ϊ��;
	strcpy(req.AffectedSOPClassUID,abstractSyntax);
	req.DataSetType=DIMSE_DATASET_PRESENT;
	req.Priority=DIMSE_PRIORITY_LOW;
	//����Ҫ��ѯ����ϢΪ��ʱ���������ѯ����л᷵��
	DcmDataset* dataset=new DcmDataset();
	InsertQueryItems(dataset,"A^B^C");
	//��ֵ�Զ���Ļص�����������Ǹûص������п��Խ��������Ϣ�Ĳ���
	ZSCFindCallback zsCallback;
	DcmFindSCUCallback* callback=&zsCallback;
	callback->setAssociation(assoc);
	callback->setPresentationContextID(presID);
	/* as long as no error occured and the counter does not equal 0 */
	cond = EC_Normal;
	while (cond.good())
	{
		DcmDataset *statusDetail = NULL;

		/* complete preparation of C-FIND-RQ message */
		req.MessageID = assoc->nextMsgID++;

		/* finally conduct transmission of data */
		cond = DIMSE_findUser(assoc, presID, &req, dataset,
			progressCallback, callback, DIMSE_BLOCKING, timeout,
			&rsp, &statusDetail);
		//�����˲�ѯ��������ģʽ��DIMSE_BLOCKING
		//�������ӳ�ʱΪ50



		/*
		 *����쳣�б�
		 *
		 */
		cond=EC_EndOfStream;//�����쳣������
	}

	return 0;
}

