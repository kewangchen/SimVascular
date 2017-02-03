#include "svSegmentation2D.h"
#include "ui_svSegmentation2D.h"
#include "ui_svLevelSet2DWidget.h"
#include "ui_svLoftParamWidget.h"

#include "svPath.h"
#include "svSegmentationUtils.h"
#include "svMath3.h"

#include "svContourGroup.h"
#include "svContourGroupDataInteractor.h"
#include "svContourCircle.h"
#include "svContourEllipse.h"
#include "svContourPolygon.h"
#include "svContourSplinePolygon.h"
#include "svContourOperation.h"
#include "svContourModel.h"
#include "svContourModelThresholdInteractor.h"

#include "svModelUtils.h"

#include <mitkOperationEvent.h>
#include <mitkUndoController.h>
#include <mitkStatusBar.h>
#include <mitkProgressBar.h>
#include <mitkNodePredicateDataType.h>

#include <usModuleRegistry.h>

// Qt
#include <QMessageBox>
#include <QInputDialog>

#include <iostream>
using namespace std;

#include <math.h>

const QString svSegmentation2D::EXTENSION_ID = "org.sv.views.segmentation2d";

svSegmentation2D::svSegmentation2D() :
    ui(new Ui::svSegmentation2D)
{
    m_ContourGroupChangeObserverTag=-1;
    m_ContourGroup=NULL;
    m_Path=NULL;
    m_Image=NULL;
    m_cvImage=NULL;
    m_LoftWidget=NULL;
    m_LSParamWidget=NULL;
    m_StartLoftContourGroupObserverTag=-1;
    m_StartLoftContourGroupObserverTag2=-1;
    m_StartChangingContourObserverTag=-1;
    m_EndChangingContourObserverTag=-1;
    m_ContourChanging=false;

    m_ManualMenu=NULL;
    m_CopyContour=NULL;
}

svSegmentation2D::~svSegmentation2D()
{
    delete ui;

    if(m_LoftWidget) delete m_LoftWidget;

//    if(m_LSParamWidget) delete m_LSParamWidget;
}

void svSegmentation2D::CreateQtPartControl( QWidget *parent )
{
    m_Parent=parent;
    ui->setupUi(parent);

    m_DisplayWidget=GetActiveStdMultiWidget();

    if(m_DisplayWidget==NULL)
    {
        parent->setEnabled(false);
        MITK_ERROR << "Plugin PathEdit Init Error: No QmitkStdMultiWidget!";
        return;
    }

    ui->resliceSlider->SetDisplayWidget(m_DisplayWidget);
    ui->resliceSlider->setCheckBoxVisible(false);
//    ui->resliceSlider->SetResliceMode(mitk::ExtractSliceFilter::RESLICE_NEAREST);
//    ui->resliceSlider->SetResliceMode(mitk::ExtractSliceFilter::RESLICE_LINEAR);
    ui->resliceSlider->SetResliceMode(mitk::ExtractSliceFilter::RESLICE_CUBIC);

    m_CurrentParamWidget=NULL;
    m_CurrentSegButton=NULL;

    QVBoxLayout* vlayout = new QVBoxLayout(ui->lsParamWidgetContainer);
    vlayout->setContentsMargins(0,0,0,0);
    vlayout->setSpacing(0);
    ui->lsParamWidgetContainer->setLayout(vlayout);
    m_LSParamWidget=new svLevelSet2DWidget(parent);
    vlayout->addWidget(m_LSParamWidget);

    ui->lsParamWidgetContainer->hide();

    ui->thresholdWidgetContainer->hide();
    ui->smoothWidget->hide();
    ui->splineWidget->hide();
    ui->batchWidget->hide();

    parent->setMinimumWidth(400);
//    parent->setFixedWidth(400);

    connect(ui->btnLevelSet, SIGNAL(clicked()), this, SLOT(CreateLSContour()) );
    connect(ui->btnThreshold, SIGNAL(clicked()), this, SLOT(CreateThresholdContour()) );
    connect(ui->btnCircle, SIGNAL(clicked()), this, SLOT(CreateCircle()) );
    connect(ui->btnEllipse, SIGNAL(clicked()), this, SLOT(CreateEllipse()) );
    connect(ui->btnSplinePoly, SIGNAL(clicked()), this, SLOT(CreateSplinePoly()) );
    connect(ui->btnPolygon, SIGNAL(clicked()), this, SLOT(CreatePolygon()) );

    connect(ui->btnSmooth, SIGNAL(clicked()), this, SLOT(SmoothSelected()) );

    connect(ui->btnDelete, SIGNAL(clicked()), this, SLOT(DeleteSelected()) );
    connect(ui->listWidget,SIGNAL(clicked(const QModelIndex&)), this, SLOT(SelectItem(const QModelIndex&)) );

    connect(ui->checkBoxLoftingPreview, SIGNAL(clicked()), this, SLOT(LoftContourGroup()) );
    connect(ui->btnLoftingOptions, SIGNAL(clicked()), this, SLOT(ShowLoftWidget()) );

    m_LoftWidget=new svLoftParamWidget();
    m_LoftWidget->move(400,400);
    m_LoftWidget->hide();
    connect(m_LoftWidget->ui->btnOK, SIGNAL(clicked()), this, SLOT(OKLofting()) );
    connect(m_LoftWidget->ui->btnApply, SIGNAL(clicked()), this, SLOT(ApplyLofting()) );
    connect(m_LoftWidget->ui->btnClose, SIGNAL(clicked()), this, SLOT(HideLoftWidget()) );

    connect(ui->resliceSlider,SIGNAL(resliceSizeChanged(double)), this, SLOT(UpdatePathResliceSize(double)) );

//    m_ManualMenu=new QMenu(ui->btnManual);

//    QAction* createManualCircleAction=m_ManualMenu->addAction("Circle");
//    QAction* createManualEllipseAction=m_ManualMenu->addAction("Ellipse");
//    QAction* createManualSplinePolyAction=m_ManualMenu->addAction("SplinePoly");
//    QAction* createManualPolygonAction=m_ManualMenu->addAction("Polygon");

//    connect( createManualCircleAction, SIGNAL( triggered(bool) ) , this, SLOT( CreateManualCircle(bool) ) );
//    connect( createManualEllipseAction, SIGNAL( triggered(bool) ) , this, SLOT( CreateManualEllipse(bool) ) );
//    connect( createManualSplinePolyAction, SIGNAL( triggered(bool) ) , this, SLOT( CreateManualSplinePoly(bool) ) );
//    connect( createManualPolygonAction, SIGNAL( triggered(bool) ) , this, SLOT( CreateManualPolygon(bool) ) );

//    connect( ui->btnManual, SIGNAL(clicked()), this, SLOT(ManualContextMenuRequested()) );

    connect( ui->btnCircle, SIGNAL(customContextMenuRequested(const QPoint&))
             , this, SLOT(ManualCircleContextMenuRequested(const QPoint&)) );
    connect( ui->btnEllipse, SIGNAL(customContextMenuRequested(const QPoint&))
             , this, SLOT(ManualEllipseContextMenuRequested(const QPoint&)) );
    connect( ui->btnSplinePoly, SIGNAL(customContextMenuRequested(const QPoint&))
             , this, SLOT(ManualSplinePolyContextMenuRequested(const QPoint&)) );
    connect( ui->btnPolygon, SIGNAL(customContextMenuRequested(const QPoint&))
             , this, SLOT(ManualPolygonContextMenuRequested(const QPoint&)) );

    connect(ui->btnCopy, SIGNAL(clicked()), this, SLOT(CopyContour()) );
    connect(ui->btnPaste, SIGNAL(clicked()), this, SLOT(PasteContour()) );
}

void svSegmentation2D::Visible()
{
//    ui->resliceSlider->turnOnReslice(true);
    OnSelectionChanged(GetDataManagerSelection());
}

void svSegmentation2D::Hidden()
{
    ui->resliceSlider->turnOnReslice(false);
    ClearAll();
}

//bool svSegmentation2D::IsExclusiveFunctionality() const
//{
//    return true;
//}

int svSegmentation2D::GetTimeStep()
{
    mitk::SliceNavigationController* timeNavigationController = NULL;
    if(m_DisplayWidget)
    {
        timeNavigationController=m_DisplayWidget->GetTimeNavigationController();
    }

    if(timeNavigationController)
        return timeNavigationController->GetTime()->GetPos();
    else
        return 0;

}

void svSegmentation2D::OnSelectionChanged(std::vector<mitk::DataNode*> nodes )
{
//    if(!IsActivated())
    if(!IsVisible())
    {
        return;
    }

    if(nodes.size()==0)
    {
        ui->resliceSlider->turnOnReslice(false);
        ClearAll();
        m_Parent->setEnabled(false);
        return;
    }

    mitk::DataNode::Pointer groupNode=nodes.front();

    if(m_ContourGroupNode==groupNode)
    {
        ui->resliceSlider->turnOnReslice(true);
        return;
    }

    ClearAll();

    m_ContourGroupNode=groupNode;
    m_ContourGroup=dynamic_cast<svContourGroup*>(groupNode->GetData());
    if(!m_ContourGroup)
    {
        ui->resliceSlider->turnOnReslice(false);
        ClearAll();
        m_Parent->setEnabled(false);
        return;
    }

    m_Parent->setEnabled(true);

//    std::string groupPathName=m_ContourGroup->GetPathName();
    int  groupPathID=m_ContourGroup->GetPathID();

//    mitk::DataNode::Pointer pathNode=NULL;
    mitk::DataNode::Pointer imageNode=NULL;
    mitk::NodePredicateDataType::Pointer isProjFolder = mitk::NodePredicateDataType::New("svProjectFolder");
    mitk::DataStorage::SetOfObjects::ConstPointer rs=GetDataStorage()->GetSources (m_ContourGroupNode,isProjFolder,false);

    if(rs->size()>0)
    {
        mitk::DataNode::Pointer projFolderNode=rs->GetElement(0);

        rs=GetDataStorage()->GetDerivations (projFolderNode,mitk::NodePredicateDataType::New("svImageFolder"));
        if(rs->size()>0)
        {

            mitk::DataNode::Pointer imageFolderNode=rs->GetElement(0);
            rs=GetDataStorage()->GetDerivations(imageFolderNode);
            if(rs->size()<1) return;
            imageNode=rs->GetElement(0);
            m_Image= dynamic_cast<mitk::Image*>(imageNode->GetData());

        }

        rs=GetDataStorage()->GetDerivations(projFolderNode,mitk::NodePredicateDataType::New("svPathFolder"));
        if (rs->size()>0)
        {
            mitk::DataNode::Pointer pathFolderNode=rs->GetElement(0);
            rs=GetDataStorage()->GetDerivations(pathFolderNode);

            for(int i=0;i<rs->size();i++)
            {
                svPath* path=dynamic_cast<svPath*>(rs->GetElement(i)->GetData());

                if(path&&groupPathID==path->GetPathID())
                {
                    m_Path=path;
                    break;
                }
            }

        }

    }

    if(!m_Image){
        QMessageBox::warning(NULL,"No image found for this project","Make sure the image is loaded!");
        return;
    }

    if(!m_Path){
        QMessageBox::warning(NULL,"No path found for this contour group","Make sure the path for the contour group exits!");
        return;
    }

    ui->labelGroupName->setText(QString::fromStdString(m_ContourGroupNode->GetName()));

    UpdateContourList();

    m_cvImage=svSegmentationUtils::image2cvStrPts(m_Image);

    int timeStep=GetTimeStep();
    svPathElement* pathElement=m_Path->GetPathElement(timeStep);
    if(pathElement==NULL)
        return;

    std::vector<svPathElement::svPathPoint> pathPoints=pathElement->GetPathPoints();
    std::vector<mitk::Point3D> pathPosPoints=pathElement->GetPathPosPoints();

    for(int i=0;i<m_ContourGroup->GetSize(timeStep);i++)
    {
        svContour* contour=m_ContourGroup->GetContour(i,timeStep);
        if(contour==NULL) continue;

        int insertingIndex=svMath3::GetInsertintIndexByDistance(pathPosPoints, contour->GetPathPosPoint(), false);
        if(insertingIndex!=-2)
        {
            svPathElement::svPathPoint pp1=pathElement->GetPathPoint(insertingIndex);
            svPathElement::svPathPoint pp2=contour->GetPathPoint();

            //if the two path points have the same position and tangent, do not add
            if(pp1.pos[0]==pp2.pos[0] && pp1.pos[1]==pp2.pos[1] && pp1.pos[2]==pp2.pos[2]
               && pp1.tangent[0]==pp2.tangent[0] && pp1.tangent[1]==pp2.tangent[1] && pp1.tangent[2]==pp2.tangent[2])
                continue;

            pathPosPoints.insert(pathPosPoints.begin()+insertingIndex,contour->GetPathPosPoint());
            pathPoints.insert(pathPoints.begin()+insertingIndex,contour->GetPathPoint());
        }
    }

    //set tag index for contours in the group
    for(int i=0;i<m_ContourGroup->GetSize(timeStep);i++)
    {
        svContour* contour=m_ContourGroup->GetContour(i,timeStep);
        if(contour==NULL) continue;

        for(int j=0;j<pathPoints.size();j++)
        {
            if(pathPoints[j].pos==contour->GetPathPosPoint())
            {
                contour->SetTagIndex(j);
                break;
            }
        }
    }
    m_PathPoints=pathPoints;

    //set resice slider
    ui->resliceSlider->setPathPoints(pathPoints);
    ui->resliceSlider->setImageNode(imageNode);
    double resliceSize=m_ContourGroup->GetResliceSize();
    if(resliceSize==0)
    {
        resliceSize=5.0;
        m_ContourGroup->SetResliceSize(resliceSize);
    }
    ui->resliceSlider->setResliceSize(resliceSize);
    ui->resliceSlider->updateReslice();

    m_DataInteractor = svContourGroupDataInteractor::New();
    m_DataInteractor->SetInteraction3D(true);
    m_DataInteractor->LoadStateMachine("svContourGroupInteraction.xml", us::ModuleRegistry::GetModule("svSegmentation"));
    m_DataInteractor->SetEventConfig("svSegmentationConfig.xml", us::ModuleRegistry::GetModule("svSegmentation"));
    m_DataInteractor->SetDataNode(m_ContourGroupNode);

    //Add Observer
    itk::SimpleMemberCommand<svSegmentation2D>::Pointer groupChangeCommand = itk::SimpleMemberCommand<svSegmentation2D>::New();
    groupChangeCommand->SetCallbackFunction(this, &svSegmentation2D::UpdateContourList);
    m_ContourGroupChangeObserverTag = m_ContourGroup->AddObserver( svContourGroupEvent(), groupChangeCommand);

    itk::SimpleMemberCommand<svSegmentation2D>::Pointer loftCommand = itk::SimpleMemberCommand<svSegmentation2D>::New();
    loftCommand->SetCallbackFunction(this, &svSegmentation2D::LoftContourGroup);
    m_StartLoftContourGroupObserverTag = m_ContourGroup->AddObserver( svContourGroupChangeEvent(), loftCommand);
    m_StartLoftContourGroupObserverTag2 = m_ContourGroup->AddObserver( svContourChangeEvent(), loftCommand);

    itk::SimpleMemberCommand<svSegmentation2D>::Pointer flagOnCommand = itk::SimpleMemberCommand<svSegmentation2D>::New();
    flagOnCommand->SetCallbackFunction(this, &svSegmentation2D::ContourChangingOn);
    m_StartChangingContourObserverTag = m_ContourGroup->AddObserver( StartChangingContourEvent(), flagOnCommand);

    itk::SimpleMemberCommand<svSegmentation2D>::Pointer flagOffCommand = itk::SimpleMemberCommand<svSegmentation2D>::New();
    flagOffCommand->SetCallbackFunction(this, &svSegmentation2D::ContourChangingOff);
    m_EndChangingContourObserverTag = m_ContourGroup->AddObserver( EndChangingContourEvent(), flagOffCommand);

    double range[2];
    m_cvImage->GetVtkStructuredPoints()->GetScalarRange(range);
    ui->sliderThreshold->setMinimum(range[0]);
    ui->sliderThreshold->setMaximum(range[1]);

    bool lofting=false;
    m_ContourGroupNode->GetBoolProperty("lofting",lofting);
    ui->checkBoxLoftingPreview->setChecked(lofting);

    ui->resliceSlider->turnOnReslice(true);
}

double svSegmentation2D::GetVolumeImageSpacing()
{
    mitk::Vector3D spacing=m_Image->GetGeometry()->GetSpacing();
    double minSpacing=std::min(spacing[0],std::min(spacing[1],spacing[2]));
    return minSpacing;
}

void svSegmentation2D::InsertContour(svContour* contour, int contourIndex)
{
    if(m_ContourGroup&&contour&&contourIndex>-2){

        m_ContourGroup->DeselectContours();

        contour->SetSelected(true);

        int timeStep=GetTimeStep();
        svContourOperation* doOp = new svContourOperation(svContourOperation::OpINSERTCONTOUR,timeStep,contour,contourIndex);

        svContourOperation *undoOp = new svContourOperation(svContourOperation::OpREMOVECONTOUR,timeStep, contour, contourIndex);
        mitk::OperationEvent *operationEvent = new mitk::OperationEvent(m_ContourGroup, doOp, undoOp, "Insert Contour");

        mitk::UndoController::GetCurrentUndoModel()->SetOperationEvent( operationEvent );

        m_ContourGroup->ExecuteOperation(doOp);

        mitk::RenderingManager::GetInstance()->RequestUpdateAll();

    }
}

void svSegmentation2D::InsertContourByPathPosPoint(svContour* contour)
{
    if(m_ContourGroup&&contour)
    {
//        mitk::OperationEvent::IncCurrObjectEventId();

        int index=m_ContourGroup->GetContourIndexByPathPosPoint(contour->GetPathPosPoint());
        if(index!=-2)
        {
            SetContour(index, contour);
        }else{
            for(int i=0;i<m_PathPoints.size();i++)
            {
                if(m_PathPoints[i].pos==contour->GetPathPosPoint())
                    contour->SetTagIndex(i);
            }

//            index=m_ContourGroup->GetInsertingContourIndexByPathPosPoint(contour->GetPathPosPoint());
            index=m_ContourGroup->GetInsertingContourIndexByTagIndex(contour->GetTagIndex());
            InsertContour(contour,index);
        }
    }

}

void svSegmentation2D::SetContour(int contourIndex, svContour* newContour)
{
    if(m_ContourGroup&&contourIndex>-2)
    {
        svContour* originalContour=m_ContourGroup->GetContour(contourIndex);

        int timeStep=GetTimeStep();
        svContourOperation* doOp = new svContourOperation(svContourOperation::OpSETCONTOUR,timeStep,newContour,contourIndex);

        svContourOperation *undoOp = new svContourOperation(svContourOperation::OpSETCONTOUR,timeStep, originalContour, contourIndex);
        mitk::OperationEvent *operationEvent = new mitk::OperationEvent(m_ContourGroup, doOp, undoOp, "Set Contour");

        mitk::UndoController::GetCurrentUndoModel()->SetOperationEvent( operationEvent );

        m_ContourGroup->ExecuteOperation(doOp);

        mitk::RenderingManager::GetInstance()->RequestUpdateAll();

    }
}

void svSegmentation2D::RemoveContour(int contourIndex)
{
    if(m_ContourGroup&&contourIndex>-2)
    {
        svContour* contour=m_ContourGroup->GetContour(contourIndex);

        int timeStep=GetTimeStep();
        svContourOperation* doOp = new svContourOperation(svContourOperation::OpREMOVECONTOUR,timeStep,contour,contourIndex);

        svContourOperation *undoOp = new svContourOperation(svContourOperation::OpINSERTCONTOUR,timeStep, contour, contourIndex);
        mitk::OperationEvent *operationEvent = new mitk::OperationEvent(m_ContourGroup, doOp, undoOp, "Remove Contour");

        mitk::UndoController::GetCurrentUndoModel()->SetOperationEvent( operationEvent );

        m_ContourGroup->ExecuteOperation(doOp);

        mitk::RenderingManager::GetInstance()->RequestUpdateAll();

    }
}

std::vector<int> svSegmentation2D::GetBatchList()
{
    std::vector<int> batchList;

    int maxID=ui->resliceSlider->GetSliceNumber();
    if(maxID<1) return batchList;

    QString line=ui->lineEditBatchList->text().trimmed();
    line=line.replace("begin",QString::number(0));
    line=line.replace("end",QString::number(maxID));

    QStringList list = line.split(QRegExp("[(),{}\\s+]"), QString::SkipEmptyParts);

    for(int i=0;i<list.size();i++)
    {
        QString str=list[i];

        if(str.contains(":"))
        {
            QStringList list2 = str.split(QRegExp(":"));

            if(list2.size()==0 ||list2.size()>3) continue;

            if(list2.size()==1)
            {
                bool ok;
                int posID=list2[0].toInt(&ok);
                if(ok&&posID>=0&&posID<=maxID) batchList.push_back(posID);
            }
            else
            {
                bool ok;
                int beginID, endID, interval;

                beginID=list2[0].toInt(&ok);
                if(!ok) continue;

                if(list2.size()==2)
                {
                    endID=list2[1].toInt(&ok);
                    if(!ok) continue;
                    interval=1;
                }else{
                    interval=list2[1].toInt(&ok);
                    if(!ok) continue;
                    endID=list2[2].toInt(&ok);
                    if(!ok) continue;
                }

                if(beginID<0 || beginID>maxID || endID<0 || endID>maxID) continue;

                for(int j=beginID;j<=endID;j=j+interval)
                    batchList.push_back(j);

                if(batchList.back()!=endID)
                    batchList.push_back(endID);
            }

        }
        else
        {
            bool ok;
            int posID=str.toInt(&ok);
            if(ok&&posID>=0&&posID<=maxID) batchList.push_back(posID);
        }

    }

    return batchList;

}

svContour* svSegmentation2D::PostprocessContour(svContour* contour)
{
    svContour* contourNew=contour;

    int smoothNumber=0;
    if(ui->checkBoxSmooth->isChecked())
    {
        smoothNumber=ui->spinBoxSmoothNumber->value();
    }

    int splineControlNumber=0;
    if(ui->checkBoxSpline->isChecked())
    {
        splineControlNumber=ui->spinBoxControlNumber->value();
    }

    if(smoothNumber>0)
    {
        contourNew=contour->CreateSmoothedContour(smoothNumber);
    }

    if(splineControlNumber>1)
    {
        contourNew=svContourSplinePolygon::CreateByFitting(contourNew,splineControlNumber);
        contourNew->SetSubdivisionType(svContour::CONSTANT_SPACING);
        contourNew->SetSubdivisionSpacing(GetVolumeImageSpacing());
    }

    return contourNew;
}

void svSegmentation2D::CreateContours(SegmentationMethod method)
{
    bool usingBatch;
    std::vector<int> posList;
    if(ui->checkBoxBatch->isChecked())
    {
        usingBatch=true;
        posList=GetBatchList();

        if(posList.size()>50)
        {
            QString msg = "There will be "+ QString::number(posList.size()) + " segmentations to do. Are you sure?";
            if (QMessageBox::question(NULL, "Warning", msg,
                                      QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            {
              return;
            }
        }

    }
    else
    {
        usingBatch=false;
        posList.push_back(ui->resliceSlider->getCurrentSliceIndex());
    }

    if(posList.size()>0)
        mitk::OperationEvent::IncCurrObjectEventId();

    mitk::ProgressBar::GetInstance()->AddStepsToDo(posList.size());

    for(int i=0;i<posList.size();i++)
    {
        int posID=posList[i];

        if(usingBatch)
        {
//            ui->resliceSlider->setSlicePos(posID);
        }

        svContour* contour=NULL;

        switch(method)
        {
            case LEVELSET_METHOD:
            {
                svSegmentationUtils::svLSParam tmpLSParam = m_LSParamWidget->GetLSParam();
                contour=svSegmentationUtils::CreateLSContour(ui->resliceSlider->getPathPoint(posID),m_cvImage->GetVtkStructuredPoints(),&tmpLSParam,ui->resliceSlider->getResliceSize());
                break;
            }
            case THRESHOLD_METHOD:
                contour=svSegmentationUtils::CreateThresholdContour(ui->resliceSlider->getPathPoint(posID),m_cvImage->GetVtkStructuredPoints(),ui->sliderThreshold->value(), ui->resliceSlider->getResliceSize());
                break;
            default:
                break;
        }

        if(contour && contour->GetContourPointNumber()>2)
        {
            contour=PostprocessContour(contour);

            InsertContourByPathPosPoint(contour);

            LoftContourGroup();

            mitk::StatusBar::GetInstance()->DisplayText("contour added");
        }
        else
        {
            if(contour)
                delete contour;

            if(posList.size()==1)
                QMessageBox::warning(NULL,"No Valid Contour Created","Contour not created and added since it's invalid");
        }

        mitk::ProgressBar::GetInstance()->Progress(1);
    }

    //LoftContourGroup();
}

void svSegmentation2D::SetSecondaryWidgetsVisible(bool visible)
{
    ui->smoothWidget->setVisible(visible);
    ui->splineWidget->setVisible(visible);
    ui->batchWidget->setVisible(visible);
}

void svSegmentation2D::ResetGUI()
{
    if(m_CurrentParamWidget)
    {
        m_CurrentParamWidget->hide();
        m_CurrentParamWidget=NULL;
    }

    if(m_CurrentSegButton)
        m_CurrentSegButton->setStyleSheet("");

    FinishPreview();//remove node, data, interaction for interactive threshold

    if(m_ContourGroup)
        m_ContourGroup->RemoveInvalidContours(GetTimeStep());

    m_ContourChanging=false;
}

void svSegmentation2D::CreateLSContour()
{
    if(m_CurrentParamWidget==NULL||m_CurrentParamWidget!=ui->lsParamWidgetContainer)
    {
        ResetGUI();

        m_CurrentParamWidget=ui->lsParamWidgetContainer;
        m_CurrentParamWidget->show();

        m_CurrentSegButton=ui->btnLevelSet;

        SetSecondaryWidgetsVisible(true);

        return;
    }

    CreateContours(LEVELSET_METHOD);

}

void svSegmentation2D::CreateThresholdContour()
{
    if(m_CurrentParamWidget==NULL||m_CurrentParamWidget!=ui->thresholdWidgetContainer)
    {
        ResetGUI();

        m_CurrentParamWidget=ui->thresholdWidgetContainer;
        m_CurrentParamWidget->show();

        m_CurrentSegButton=ui->btnThreshold;

        SetSecondaryWidgetsVisible(true);

        return;
    }

    if(ui->checkBoxPresetThreshold->isChecked())
    {
        CreateContours(THRESHOLD_METHOD);
        return;
    }

    if(m_CurrentSegButton->styleSheet().trimmed()!="")
        return;

    m_CurrentSegButton->setStyleSheet("background-color: lightskyblue");

    cvStrPts*  strPts=svSegmentationUtils::GetSlicevtkImage(ui->resliceSlider->getCurrentPathPoint(), m_cvImage->GetVtkStructuredPoints(), ui->resliceSlider->getResliceSize());

    svContour* contour=new svContour();
    contour->SetPathPoint(ui->resliceSlider->getCurrentPathPoint());
    contour->SetPlaced(true);
    contour->SetMethod("Threshold");
    contour->SetVtkImageSlice(strPts->GetVtkStructuredPoints());

    m_PreviewContourModel = svContourModel::New();
    m_PreviewContourModel->SetContour(contour);

    m_PreviewDataNode = mitk::DataNode::New();
    m_PreviewDataNode->SetData(m_PreviewContourModel);
    m_PreviewDataNode->SetName("PreviewContour");
    m_PreviewDataNode->SetBoolProperty("helper object", true);

    //GetDataStorage()->Add(m_PreviewDataNode);
    GetDataStorage()->Add(m_PreviewDataNode, m_ContourGroupNode);

    m_PreviewDataNodeInteractor= svContourModelThresholdInteractor::New();
    m_PreviewDataNodeInteractor->LoadStateMachine("svContourModelThresholdInteraction.xml", us::ModuleRegistry::GetModule("svSegmentation"));
    m_PreviewDataNodeInteractor->SetEventConfig("svSegmentationConfig.xml", us::ModuleRegistry::GetModule("svSegmentation"));
    m_PreviewDataNodeInteractor->SetDataNode(m_PreviewDataNode);

    itk::SimpleMemberCommand<svSegmentation2D>::Pointer previewFinished = itk::SimpleMemberCommand<svSegmentation2D>::New();
    previewFinished->SetCallbackFunction(this, &svSegmentation2D::FinishPreview);
    m_PreviewContourModelObserverFinishTag = m_PreviewContourModel->AddObserver( EndInteractionContourModelEvent(), previewFinished);

    itk::SimpleMemberCommand<svSegmentation2D>::Pointer previewUpdating = itk::SimpleMemberCommand<svSegmentation2D>::New();
    previewUpdating->SetCallbackFunction(this, &svSegmentation2D::UpdatePreview);
    m_PreviewContourModelObserverUpdateTag = m_PreviewContourModel->AddObserver( UpdateInteractionContourModelEvent(), previewUpdating);
}

void svSegmentation2D::UpdatePreview()
{
    if(m_PreviewContourModel.IsNull())
         return;

    svContourModelThresholdInteractor* interactor=dynamic_cast<svContourModelThresholdInteractor*>( m_PreviewDataNodeInteractor.GetPointer());
    if(interactor)
    {
        ui->sliderThreshold->setValue(interactor->GetCurrentValue());
    }
}

void svSegmentation2D::FinishPreview()
{
    if(m_PreviewContourModel.IsNull())
         return;

    int timeStep=GetTimeStep();

    svContour* contour=m_PreviewContourModel->GetContour(timeStep);

    if(contour && contour->GetContourPointNumber()>2)
    {
        contour=PostprocessContour(contour);

        mitk::OperationEvent::IncCurrObjectEventId();

        InsertContourByPathPosPoint(contour);

        LoftContourGroup();
    }
    else
    {
        if(contour)
            delete contour;

//        QMessageBox::warning(NULL,"No Valid Contour Created","Contour not created and added since it's invalid");
    }

    if( m_PreviewContourModelObserverFinishTag)
    {
        m_PreviewContourModel->RemoveObserver(m_PreviewContourModelObserverFinishTag);
    }

    if( m_PreviewContourModelObserverUpdateTag)
    {
        m_PreviewContourModel->RemoveObserver(m_PreviewContourModelObserverUpdateTag);
    }

    if(m_PreviewDataNode)
    {
        m_PreviewDataNode->SetDataInteractor(NULL);
        m_PreviewDataNodeInteractor=NULL;
        GetDataStorage()->Remove(m_PreviewDataNode);
        m_PreviewDataNode=NULL;
    }

    m_PreviewContourModel=NULL;
    ui->btnThreshold->setStyleSheet("");
}

//void svSegmentation2D::CreateEllipse()
//{
//    ResetGUI();

//    m_CurrentSegButton=ui->btnEllipse;
//    m_CurrentSegButton->setStyleSheet("background-color: lightskyblue");

//    SetSecondaryWidgetsVisible(false);

//    svContour* contour=new svContourEllipse();
//    contour->SetSubdivisionType(svContour::CONSTANT_SPACING);
//    contour->SetSubdivisionSpacing(GetVolumeImageSpacing());
//    contour->SetPathPoint(ui->resliceSlider->getCurrentPathPoint());

//    mitk::OperationEvent::IncCurrObjectEventId();

//    m_ContourChanging=true;

//    InsertContourByPathPosPoint(contour);
//}

void svSegmentation2D::CreateEllipse()
{
    ResetGUI();

    SetSecondaryWidgetsVisible(false);

    int index=m_ContourGroup->GetContourIndexByPathPosPoint(ui->resliceSlider->getCurrentPathPoint().pos);

    svContour* existingContour=m_ContourGroup->GetContour(index);

    svContour* contour=NULL;
    if(existingContour && existingContour->GetContourPointNumber()>2)
    {
        contour=svContourEllipse::CreateByFitting(existingContour);
    }else{

        m_CurrentSegButton=ui->btnEllipse;
        m_CurrentSegButton->setStyleSheet("background-color: lightskyblue");

        contour=new svContourEllipse();
        contour->SetPathPoint(ui->resliceSlider->getCurrentPathPoint());

        m_ContourChanging=true;
    }

    contour->SetSubdivisionType(svContour::CONSTANT_SPACING);
    contour->SetSubdivisionSpacing(GetVolumeImageSpacing());

    mitk::OperationEvent::IncCurrObjectEventId();

    InsertContourByPathPosPoint(contour);
}

void svSegmentation2D::CreateCircle()
{
    ResetGUI();

    SetSecondaryWidgetsVisible(false);

    int index=m_ContourGroup->GetContourIndexByPathPosPoint(ui->resliceSlider->getCurrentPathPoint().pos);

    svContour* existingContour=m_ContourGroup->GetContour(index);

    svContour* contour=NULL;
    if(existingContour && existingContour->GetContourPointNumber()>2)
    {
        contour=svContourCircle::CreateByFitting(existingContour);
    }else{

        m_CurrentSegButton=ui->btnCircle;
        m_CurrentSegButton->setStyleSheet("background-color: lightskyblue");

        contour=new svContourCircle();
        contour->SetPathPoint(ui->resliceSlider->getCurrentPathPoint());

        m_ContourChanging=true;
    }

    contour->SetSubdivisionType(svContour::CONSTANT_SPACING);
    contour->SetSubdivisionSpacing(GetVolumeImageSpacing());

    mitk::OperationEvent::IncCurrObjectEventId();

    InsertContourByPathPosPoint(contour);
}

void svSegmentation2D::CreateSplinePoly()
{
    ResetGUI();

    SetSecondaryWidgetsVisible(false);

    int index=m_ContourGroup->GetContourIndexByPathPosPoint(ui->resliceSlider->getCurrentPathPoint().pos);

    svContour* existingContour=m_ContourGroup->GetContour(index);

    svContour* contour;
    if(existingContour && existingContour->GetContourPointNumber()>2)
    {
        int splineControlNumber=ui->spinBoxControlNumber->value();
        contour=svContourSplinePolygon::CreateByFitting(existingContour,splineControlNumber);
    }else{

        m_CurrentSegButton=ui->btnSplinePoly;
        m_CurrentSegButton->setStyleSheet("background-color: lightskyblue");

        contour=new svContourSplinePolygon();
        contour->SetClosed(false);
        contour->SetPathPoint(ui->resliceSlider->getCurrentPathPoint());

        m_ContourChanging=true;
    }

    contour->SetSubdivisionType(svContour::CONSTANT_SPACING);
    contour->SetSubdivisionSpacing(GetVolumeImageSpacing());

    mitk::OperationEvent::IncCurrObjectEventId();

    InsertContourByPathPosPoint(contour);
}

void svSegmentation2D::CreatePolygon()
{
    ResetGUI();

    m_CurrentSegButton=ui->btnPolygon;
    m_CurrentSegButton->setStyleSheet("background-color: lightskyblue");

    SetSecondaryWidgetsVisible(false);

    svContour* contour=new svContourPolygon();
    contour->SetClosed(false);
    contour->SetSubdivisionType(svContour::CONSTANT_SPACING);
    contour->SetSubdivisionSpacing(GetVolumeImageSpacing());

    contour->SetPathPoint(ui->resliceSlider->getCurrentPathPoint());

    mitk::OperationEvent::IncCurrObjectEventId();

    m_ContourChanging=true;

    InsertContourByPathPosPoint(contour);
}

void svSegmentation2D::SmoothSelected()
{
//    int fourierNumber=12;
    int fourierNumber=ui->spinBoxSmoothNumber->value();

    QModelIndexList selectedRows=ui->listWidget->selectionModel()->selectedRows();

    if(selectedRows.isEmpty())
        return;

    int index= selectedRows.front().row();

    svContour* smoothedContour=m_ContourGroup->GetContour(index)->CreateSmoothedContour(fourierNumber);

    InsertContourByPathPosPoint(smoothedContour);

    LoftContourGroup();
}

void svSegmentation2D::DeleteSelected()
{
    QModelIndexList selectedRows=ui->listWidget->selectionModel()->selectedRows();
    if(selectedRows.size()>0)
    {
        RemoveContour(selectedRows.front().row());
    }

    LoftContourGroup();
}

void svSegmentation2D::SelectItem(const QModelIndex & idx)
{
    int index=idx.row();

    if(m_ContourGroup)
    {
        if(m_ContourGroup->GetSelectedContourIndex()==-2 || !m_ContourGroup->IsContourSelected(index))
        {
            if(m_ContourGroup->GetSelectedContourIndex()!=-2)
            {
                m_ContourGroup->DeselectContours();
            }

            m_ContourGroup->SetContourSelected(index,true);
            svContour* contour=m_ContourGroup->GetContour(index);
            if(contour)
            {
                ui->resliceSlider->moveToPathPosPoint(contour->GetPathPosPoint());
            }
        }
        else
        {
            m_ContourGroup->SetContourSelected(index,false);
        }

        mitk::RenderingManager::GetInstance()->ForceImmediateUpdateAll();
    }

}

void svSegmentation2D::NodeChanged(const mitk::DataNode* node)
{
}

void svSegmentation2D::NodeAdded(const mitk::DataNode* node)
{
}

void svSegmentation2D::NodeRemoved(const mitk::DataNode* node)
{
    OnSelectionChanged(GetDataManagerSelection());
}

void svSegmentation2D::ClearAll()
{
    //Remove Observer
    if(m_ContourGroup && m_ContourGroupChangeObserverTag!=-1)
    {
        m_ContourGroup->RemoveObserver(m_ContourGroupChangeObserverTag);
        m_ContourGroupChangeObserverTag=-1;
    }

    if(m_ContourGroup && m_StartLoftContourGroupObserverTag!=-1)
    {
        m_ContourGroup->RemoveObserver(m_StartLoftContourGroupObserverTag);
        m_StartLoftContourGroupObserverTag=-1;
    }

    if(m_ContourGroup && m_StartLoftContourGroupObserverTag2!=-1)
    {
        m_ContourGroup->RemoveObserver(m_StartLoftContourGroupObserverTag2);
        m_StartLoftContourGroupObserverTag2=-1;
    }

    if(m_ContourGroup && m_StartChangingContourObserverTag!=-1)
    {
        m_ContourGroup->RemoveObserver(m_StartChangingContourObserverTag);
        m_StartChangingContourObserverTag=-1;
    }

    if(m_ContourGroup && m_EndChangingContourObserverTag!=-1)
    {
        m_ContourGroup->RemoveObserver(m_EndChangingContourObserverTag);
        m_EndChangingContourObserverTag=-1;
    }

    if(m_ContourGroupNode)
    {
        m_ContourGroupNode->SetDataInteractor(NULL);
        m_DataInteractor=NULL;
    }

    m_ContourGroup=NULL;
    m_ContourGroupNode=NULL;
    m_Path=NULL;

    ui->labelGroupName->setText("");
    ui->listWidget->clear();

}

void svSegmentation2D::UpdateContourList()
{
    if(m_ContourGroup==NULL) return;

    int timeStep=GetTimeStep();

    ui->listWidget->clear();

    for(int index=0;index<m_ContourGroup->GetSize(timeStep);index++){

        svContour* contour=m_ContourGroup->GetContour(index,timeStep);
        if(contour)
        {
            QString item=QString::number(index)+": "+QString::fromStdString(contour->GetType())+", "+QString::fromStdString(contour->GetMethod());
            ui->listWidget->addItem(item);
        }

    }

    int selectedIndex=m_ContourGroup->GetSelectedContourIndex(timeStep);
    if(selectedIndex>-2)
    {
        QModelIndex mIndex=ui->listWidget->model()->index(selectedIndex,0);
        ui->listWidget->selectionModel()->select(mIndex, QItemSelectionModel::ClearAndSelect);
    }

}

void svSegmentation2D::LoftContourGroup()
{
    if(m_ContourChanging)
        return;

    if(m_ContourGroupNode.IsNull())
        return;

    m_LoftSurfaceNode=GetDataStorage()->GetNamedDerivedNode("Lofted",m_ContourGroupNode);

    if(ui->checkBoxLoftingPreview->isChecked())
    {
        m_ContourGroupNode->SetBoolProperty("lofting",true);

        if(m_LoftSurfaceNode.IsNull())
        {
            m_LoftSurface=mitk::Surface::New();
            m_LoftSurfaceNode = mitk::DataNode::New();
            m_LoftSurfaceNode->SetData(m_LoftSurface);
            m_LoftSurfaceNode->SetName("Lofted");
//            m_LoftSurfaceNode->SetBoolProperty("helper object", true);
            GetDataStorage()->Add(m_LoftSurfaceNode,m_ContourGroupNode);
        }
        else
        {
            m_LoftSurface=dynamic_cast<mitk::Surface*>(m_LoftSurfaceNode->GetData());
            m_LoftSurfaceNode->SetVisibility(true);
        }

        int timeStep=GetTimeStep();

        m_ContourGroup->RemoveInvalidContours(timeStep);
        if(m_ContourGroup->GetSize()>1)
            m_LoftSurface->SetVtkPolyData(svModelUtils::CreateLoftSurface(m_ContourGroup,0,0,timeStep),timeStep);

    }
    else
    {
        m_ContourGroupNode->SetBoolProperty("lofting",false);

        if(m_LoftSurfaceNode.IsNotNull())
            m_LoftSurfaceNode->SetVisibility(false);
    }

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();

}

void svSegmentation2D::ShowLoftWidget()
{
    svContourGroup::svLoftingParam *param=m_ContourGroup->GetLoftingParam();

    m_LoftWidget->ui->spinBoxSampling->setValue(param->numOutPtsInSegs);
    m_LoftWidget->ui->spinBoxNumPerSeg->setValue(param->samplePerSegment);
    m_LoftWidget->ui->checkBoxUseLinearSample->setChecked(param->useLinearSampleAlongLength==0?false:true);
    m_LoftWidget->ui->spinBoxLinearFactor->setValue(param->linearMuliplier);
    m_LoftWidget->ui->checkBoxUseFFT->setChecked(param->useFFT==0?false:true);
    m_LoftWidget->ui->spinBoxNumModes->setValue(param->numModes);

    m_LoftWidget->show();
}

void svSegmentation2D::UpdateContourGroupLoftingParam()
{
    svContourGroup::svLoftingParam *param=m_ContourGroup->GetLoftingParam();
    param->numOutPtsInSegs=m_LoftWidget->ui->spinBoxSampling->value();
    param->samplePerSegment=m_LoftWidget->ui->spinBoxNumPerSeg->value();
    param->useLinearSampleAlongLength=m_LoftWidget->ui->checkBoxUseLinearSample->isChecked()?1:0;
    param->linearMuliplier=m_LoftWidget->ui->spinBoxLinearFactor->value();
    param->useFFT=m_LoftWidget->ui->checkBoxUseFFT->isChecked()?1:0;
    param->numModes=m_LoftWidget->ui->spinBoxNumModes->value();
}

void svSegmentation2D::OKLofting()
{
    UpdateContourGroupLoftingParam();
    LoftContourGroup();
    m_LoftWidget->hide();
}

void svSegmentation2D::ApplyLofting()
{
    UpdateContourGroupLoftingParam();
    LoftContourGroup();
}

void svSegmentation2D::HideLoftWidget()
{
    m_LoftWidget->hide();
}

void svSegmentation2D::ContourChangingOn()
{
    m_ContourChanging=true;
}

void svSegmentation2D::ContourChangingOff()
{
    m_ContourChanging=false;
    if(m_CurrentSegButton)
        m_CurrentSegButton->setStyleSheet("");
}

void svSegmentation2D::UpdatePathResliceSize(double newSize)
{
    if(m_ContourGroup)
        m_ContourGroup->SetResliceSize(newSize);
}

void svSegmentation2D::ManualContextMenuRequested()
{
    m_ManualMenu->popup(QCursor::pos());
}

void svSegmentation2D::ManualCircleContextMenuRequested(const QPoint&)
{
    CreateManualCircle();
}

void svSegmentation2D::ManualEllipseContextMenuRequested(const QPoint&)
{
    CreateManualEllipse();
}

void svSegmentation2D::ManualSplinePolyContextMenuRequested(const QPoint&)
{
    CreateManualSplinePoly();
}

void svSegmentation2D::ManualPolygonContextMenuRequested(const QPoint&)
{
    CreateManualPolygon();
}

void svSegmentation2D::CreateManualCircle(bool)
{
    bool ok;
    QString text = QInputDialog::getText(m_Parent, tr("Circle Input"),
                                         tr("radius, or x y radius:"), QLineEdit::Normal,
                                         "", &ok);
    if (!ok || text.trimmed().isEmpty())
        return;

    QStringList list = text.trimmed().split(QRegExp("[(),{}\\s+]"), QString::SkipEmptyParts);
    if(list.size()!=1 && list.size()!=3)
    {
        QMessageBox::warning(m_Parent,"Input Invalid","Please provide valid input!");
        return;
    }

    mitk::Point2D centerPoint,boundaryPoint;
    centerPoint[0]=0;
    centerPoint[1]=0;
    double radius;

    for(int i=0;i<list.size();i++)
    {
        ok=false;
        if(list.size()==3 && (i==0 || i==1))
            centerPoint[i]=list[i].toDouble(&ok);
        else
            radius=list[i].toDouble(&ok);
        if(!ok)
        {
            QMessageBox::warning(m_Parent,"Input Invalid","Please provide valid input!");
            return;
        }
    }

    ResetGUI();
    SetSecondaryWidgetsVisible(false);

    svContour* contour=new svContourCircle();
    contour->SetPathPoint(ui->resliceSlider->getCurrentPathPoint());
    contour->SetPlaced(true);
    contour->SetMethod(contour->GetMethod());

    centerPoint[0]+=contour->GetPlaneGeometry()->GetSpacing()[0]*contour->GetPlaneGeometry()->GetBounds()[1]/2;
    centerPoint[1]+=contour->GetPlaneGeometry()->GetSpacing()[1]*contour->GetPlaneGeometry()->GetBounds()[3]/2;
    boundaryPoint[0]=centerPoint[0]+radius;
    boundaryPoint[1]=centerPoint[1];

    mitk::Point3D pt1,pt2;
    contour->GetPlaneGeometry()->Map(centerPoint,pt1);
    contour->GetPlaneGeometry()->Map(boundaryPoint,pt2);
    std::vector<mitk::Point3D> controlPoints;
    controlPoints.push_back(pt1);
    controlPoints.push_back(pt2);

    contour->SetControlPoints(controlPoints);

    contour->SetSubdivisionType(svContour::CONSTANT_SPACING);
    contour->SetSubdivisionSpacing(GetVolumeImageSpacing());

    mitk::OperationEvent::IncCurrObjectEventId();

    InsertContourByPathPosPoint(contour);
}

void svSegmentation2D::CreateManualEllipse(bool)
{
    bool ok;
    QString text = QInputDialog::getText(m_Parent, tr("Ellipse Input"),
                                         tr("a b, or a b theta, or x y a b, or x y a b theta:"), QLineEdit::Normal,
                                         "", &ok);
    if (!ok || text.trimmed().isEmpty())
        return;

    QStringList list = text.trimmed().split(QRegExp("[(),{}\\s+]"), QString::SkipEmptyParts);
    if(list.size()!=2 && list.size()!=3 && list.size()!=4 && list.size()!=5)
    {
        QMessageBox::warning(m_Parent,"Input Invalid","Please provide valid input!");
        return;
    }

    mitk::Point2D centerPoint,boundaryPoint1,boundaryPoint2;
    double x=0;
    double y=0;
    double a=0;
    double b=0;
    double theta=0;

    for(int i=0;i<list.size();i++)
    {
        ok=false;
        double value=list[i].toDouble(&ok);
        if(!ok)
        {
            QMessageBox::warning(m_Parent,"Input Invalid","Please provide valid input!");
            return;
        }

        if(list.size()==2)
        {
            if(i==0) a=value;
            else if(i==1) b=value;
        }

        if(list.size()==3)
        {
            if(i==0) a=value;
            else if(i==1) b=value;
            else if(i==2) theta=value;
        }

        if(list.size()==4)
        {
            if(i==0) x=value;
            else if(i==1) y=value;
            else if(i==2) a=value;
            else if(i==3) b=value;
        }

        if(list.size()==5)
        {
            if(i==0) x=value;
            else if(i==1) y=value;
            else if(i==2) a=value;
            else if(i==3) b=value;
            else if(i==4) theta=value;
        }
    }

    ResetGUI();
    SetSecondaryWidgetsVisible(false);

    svContourEllipse* contour=new svContourEllipse();
    contour->SetPathPoint(ui->resliceSlider->getCurrentPathPoint());
    contour->SetPlaced(true);
    contour->SetMethod(contour->GetMethod());
    contour->SetAsCircle(false);

    centerPoint[0]=x+contour->GetPlaneGeometry()->GetSpacing()[0]*contour->GetPlaneGeometry()->GetBounds()[1]/2;
    centerPoint[1]=y+contour->GetPlaneGeometry()->GetSpacing()[1]*contour->GetPlaneGeometry()->GetBounds()[3]/2;
    boundaryPoint1[0]=centerPoint[0]+a*cos(theta/180.0*vnl_math::pi);
    boundaryPoint1[1]=centerPoint[1]+a*sin(theta/180.0*vnl_math::pi);
    boundaryPoint2[0]=centerPoint[0]+b*cos((theta+90)/180.0*vnl_math::pi);
    boundaryPoint2[1]=centerPoint[1]+b*sin((theta+90)/180.0*vnl_math::pi);

    mitk::Point3D pt0,pt2,pt3;
    contour->GetPlaneGeometry()->Map(centerPoint,pt0);
    contour->GetPlaneGeometry()->Map(boundaryPoint1,pt2);
    contour->GetPlaneGeometry()->Map(boundaryPoint2,pt3);
    std::vector<mitk::Point3D> controlPoints;
    controlPoints.push_back(pt0);
    controlPoints.push_back(pt0);
    controlPoints.push_back(pt2);
    controlPoints.push_back(pt3);

    contour->SetControlPoints(controlPoints,false);
    contour->SetControlPoint(2,pt2);

    contour->SetSubdivisionType(svContour::CONSTANT_SPACING);
    contour->SetSubdivisionSpacing(GetVolumeImageSpacing());

    mitk::OperationEvent::IncCurrObjectEventId();

    InsertContourByPathPosPoint(contour);
}

void svSegmentation2D::CreateManualPolygonType(bool spline)
{
    bool ok;
    QString text = QInputDialog::getText(m_Parent, tr("Polygon Input"),
                                         tr("x1 y1 x2 y2 x3 y3...:"), QLineEdit::Normal,
                                         "", &ok);
    if (!ok || text.trimmed().isEmpty())
        return;

    QStringList list = text.trimmed().split(QRegExp("[(),{}\\s+]"), QString::SkipEmptyParts);
    if(list.size()%2!=0)
    {
        QMessageBox::warning(m_Parent,"Input Invalid","Please provide valid input!");
        return;
    }

    std::vector<mitk::Point2D> points;

    for(int i=0;i<list.size();i=i+2)
    {
        bool ok1=false;
        bool ok2=false;
        double value1=list[i].toDouble(&ok1);
        double value2=list[i+1].toDouble(&ok2);
        if(!ok1 || !ok2)
        {
            QMessageBox::warning(m_Parent,"Input Invalid","Please provide valid input!");
            return;
        }

        mitk::Point2D point;
        point[0]=value1;
        point[1]=value2;

        points.push_back(point);
    }

    ResetGUI();
    SetSecondaryWidgetsVisible(false);

    svContour* contour=NULL;
    if(spline)
        contour=new svContourSplinePolygon();
    else
        contour=new svContourPolygon();

    contour->SetPathPoint(ui->resliceSlider->getCurrentPathPoint());
    contour->SetPlaced(true);
    contour->SetMethod(contour->GetMethod());

    mitk::Point3D pt;
    pt[0]=0;
    pt[1]=0;
    pt[2]=0;

    std::vector<mitk::Point3D> controlPoints;
    controlPoints.push_back(pt);
    controlPoints.push_back(pt);

    double dx=contour->GetPlaneGeometry()->GetSpacing()[0]*contour->GetPlaneGeometry()->GetBounds()[1]/2;
    double dy=contour->GetPlaneGeometry()->GetSpacing()[1]*contour->GetPlaneGeometry()->GetBounds()[3]/2;

    for(int i=0;i<points.size();i++)
    {
        mitk::Point2D pt2d;
        pt2d[0]=points[i][0]+dx;
        pt2d[1]=points[i][1]+dy;
        contour->GetPlaneGeometry()->Map(pt2d,pt);
        controlPoints.push_back(pt);
    }

    contour->SetControlPoints(controlPoints);

    contour->SetSubdivisionType(svContour::CONSTANT_SPACING);
    contour->SetSubdivisionSpacing(GetVolumeImageSpacing());

    mitk::OperationEvent::IncCurrObjectEventId();

    InsertContourByPathPosPoint(contour);
}

void svSegmentation2D::CreateManualSplinePoly(bool)
{
    CreateManualPolygonType(true);
}

void svSegmentation2D::CreateManualPolygon(bool)
{
    CreateManualPolygonType(false);
}

void svSegmentation2D::CopyContour()
{
    QModelIndexList selectedRows=ui->listWidget->selectionModel()->selectedRows();
    if(selectedRows.size()>0 && m_ContourGroup)
    {
        m_CopyContour=m_ContourGroup->GetContour(selectedRows.front().row(),GetTimeStep());
    }
}

void svSegmentation2D::PasteContour()
{
    if(m_CopyContour==NULL)
        return;

    svContour* contour=m_CopyContour->Clone();
    mitk::PlaneGeometry* oldPlaneGeometry=m_CopyContour->GetPlaneGeometry();
    std::vector<mitk::Point3D> controlPoints;
    std::vector<mitk::Point3D> contourPoints;

    contour->SetPathPoint(ui->resliceSlider->getCurrentPathPoint());
    for(int i=0;i<contour->GetControlPointNumber();i++)
    {
        mitk::Point2D p2d;
        mitk::Point3D p3d;
        oldPlaneGeometry->Map(contour->GetControlPoint(i),p2d);
        contour->GetPlaneGeometry()->Map(p2d,p3d);

        controlPoints.push_back(p3d);
    }
    for(int i=0;i<contour->GetContourPointNumber();i++)
    {
        mitk::Point2D p2d;
        mitk::Point3D p3d;
        oldPlaneGeometry->Map(contour->GetContourPoint(i),p2d);
        contour->GetPlaneGeometry()->Map(p2d,p3d);

        contourPoints.push_back(p3d);
    }

    contour->SetControlPoints(controlPoints,false);
    contour->SetContourPoints(contourPoints,false);

    mitk::OperationEvent::IncCurrObjectEventId();

    InsertContourByPathPosPoint(contour);

    LoftContourGroup();
}
