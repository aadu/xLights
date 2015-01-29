#include "wx/wx.h"
#include <wx/utils.h>
#include <algorithm>
#include <wx/tokenzr.h>
#include <wx/filename.h>

#include "SequenceElements.h"
#include "TimeLine.h"


SequenceElements::SequenceElements()
{
    mSelectedTimingRow = -1;
}

SequenceElements::~SequenceElements()
{
}


void SequenceElements::AddElement(wxString &name,wxString &type,bool visible,bool collapsed,bool active)
{
    if(!ElementExists(name))
    {
        Element e(name,type,visible,collapsed,active);
        mElements.push_back(e);
    }
}

int SequenceElements::GetElementCount()
{
    return mElements.size();
}

bool SequenceElements::ElementExists(wxString elementName)
{
    for(int i=0;i<mElements.size();i++)
    {
        if(mElements[i].GetName() == elementName)
        {
            return true;
        }
    }
    return false;
}

void SequenceElements::SetViewsNode(wxXmlNode* viewsNode)
{
    mViewsNode = viewsNode;
}

wxString SequenceElements::GetViewModels(wxString viewName)
{
    wxString result="";
    for(wxXmlNode* view=mViewsNode->GetChildren(); view!=NULL; view=view->GetNext() )
    {
        if(view->GetAttribute("name")==viewName)
        {
            result = view->GetAttribute("models");
            break;
        }
    }
    return result;
}

Element* SequenceElements::GetElement(const wxString &name)
{
    for(int i=0;i<mElements.size();i++)
    {
        if(name == mElements[i].GetName())
        {
            return &mElements[i];
        }
    }
    return NULL;
}

Element* SequenceElements::GetElement(int index)
{
    if(index < mElements.size())
    {
        return &mElements[index]
;    }
    else
    {
        return nullptr;
    }
}


void SequenceElements::DeleteElement(wxString name)
{
    for(int i=0;i<mElements.size();i++)
    {
        if(name == mElements[i].GetName())
        {
            mElements.erase(mElements.begin()+i);
        }
    }
}

Row_Information_Struct* SequenceElements::GetRowInformation(int index)
{
    if(index < mRowInformation.size())
    {
        return &mRowInformation[index];
    }
    else
    {
        return NULL;
    }
}

int SequenceElements::GetRowInformationSize()
{
    return mRowInformation.size();
}

void SequenceElements::SortElements()
{
    if (mRowInformation.size()>1)
        std::sort(mElements.begin(),mElements.end(),SortElementsByIndex);
}

void SequenceElements::MoveElement(int index,int destinationIndex)
{
    if(index<destinationIndex)
    {
        mElements[index].Index = destinationIndex;
        for(int i=index+1;i<destinationIndex;i++)
        {
            mElements[i].Index = i-1;
        }
    }
    else
    {
        mElements[index].Index = destinationIndex;
        for(int i=destinationIndex;i<index;i++)
        {
            mElements[i].Index = i+1;
        }
    }
    SortElements();

}


bool SequenceElements::LoadSequencerFile(xLightsXmlFile xml_file)
{
    wxXmlDocument& seqDocument = xml_file.GetXmlDocument();

    int gridCol;

    wxXmlNode* root=seqDocument.GetRoot();

    mElements.clear();
    for(wxXmlNode* e=root->GetChildren(); e!=NULL; e=e->GetNext() )
    {
       if (e->GetName() == "DisplayElements")
       {
            for(wxXmlNode* element=e->GetChildren(); element!=NULL; element=element->GetNext() )
            {
                bool active=false;
                bool collapsed=false;
                wxString name = element->GetAttribute("name");
                wxString type = element->GetAttribute("type");
                bool visible = element->GetAttribute("visible")=='1'?true:false;

                if (type=="timing")
                {
                    active = element->GetAttribute("active")=='1'?true:false;
                }
                else
                {
                    collapsed = element->GetAttribute("collapsed")=='1'?true:false;
                }
                AddElement(name,type,visible,collapsed,active);
                // Add models for each view
                if(type=="view")
                {
                    wxString models = GetViewModels(name);
                    if(models.length()> 0)
                    {
                        wxArrayString model=wxSplit(models,',');
                        for(int m=0;m<model.size();m++)
                        {
                           wxString modelName =  model[m];
                           wxString elementType = "model";
                           AddElement(modelName,elementType,false,false,false);
                        }
                    }
                }
            }
       }
       else if (e->GetName() == "ElementEffects")
        {
            for(wxXmlNode* elementNode=e->GetChildren(); elementNode!=NULL; elementNode=elementNode->GetNext() )
            {
                if(elementNode->GetName()=="Element")
                {
                    Element* element = GetElement(elementNode->GetAttribute("name"));
                    if (element !=NULL)
                    {
                        int layerIndex=0;
                        for(wxXmlNode* effectLayerNode=elementNode->GetChildren(); effectLayerNode!=NULL; effectLayerNode=effectLayerNode->GetNext())
                        {
                            if (effectLayerNode->GetName() == "EffectLayer")
                            {
                                element->AddEffectLayer();
                                EffectLayer* effectLayer = element->GetEffectLayer(layerIndex);
                                for(wxXmlNode* effect=effectLayerNode->GetChildren(); effect!=NULL; effect=effect->GetNext())
                                {
                                    if (effect->GetName() == "Effect")
                                    {
                                        wxString effectName;
                                        wxString settings;
                                        int id;
                                        int effectIndex;
                                        bool bProtected=false;

                                        // Start time
                                        double startTime;
                                        effect->GetAttribute("startTime").ToDouble(&startTime);
                                        startTime = TimeLine::RoundToMultipleOfPeriod(startTime,mFrequency);
                                        // End time
                                        double endTime;
                                        effect->GetAttribute("endTime").ToDouble(&endTime);
                                        endTime = TimeLine::RoundToMultipleOfPeriod(endTime,mFrequency);
                                        // Protected
                                        bProtected = effect->GetAttribute("protected")=='1'?true:false;
                                        if(elementNode->GetAttribute("type") != "timing")
                                        {
                                            // Name
                                            effectName = effect->GetAttribute("name");
                                            // ID
                                            id = wxAtoi(effect->GetAttribute("id"));
                                            effectIndex = Effect::GetEffectIndex(effectName);
                                            settings = effect->GetNodeContent();
                                        }
                                        effectLayer->AddEffect(id,effectIndex,effectName,settings,startTime,endTime,EFFECT_NOT_SELECTED,bProtected);
                                    }
                                }
                            }
                            layerIndex++;
                        }
                    }
                }
            }
        }
    }
    PopulateRowInformation();
    return true;
}

void SequenceElements::SetFrequency(double frequency)
{
    mFrequency = frequency;
}


void SequenceElements::SetSelectedTimingRow(int row)
{
    mSelectedTimingRow = row;
}

int SequenceElements::GetSelectedTimingRow()
{
    return mSelectedTimingRow;
}


void SequenceElements::PopulateRowInformation()
{
    int rowIndex=0;
    int timingColorIndex=0;
    mSelectedTimingRow = -1;
    mRowInformation.clear();
    for(int i=0;i<mElements.size();i++)
    {
        if(mElements[i].GetVisible())
        {
            if (mElements[i].GetType()=="model")
            {
                if(!mElements[i].GetCollapsed())
                {
                    for(int j =0; j<mElements[i].GetEffectLayerCount();j++)
                    {
                        Row_Information_Struct ri;
                        ri.element = &mElements[i];
                        ri.Collapsed = mElements[i].GetCollapsed();
                        ri.Active = mElements[i].GetActive();
                        ri.PartOfView = false;
                        ri.colorIndex = 0;
                        ri.layerIndex = j;
                        ri.Index = rowIndex++;
                        mRowInformation.push_back(ri);
                    }
                }
                else
                {
                    Row_Information_Struct ri;
                    ri.element = &mElements[i];
                    ri.Collapsed = mElements[i].GetCollapsed();
                    ri.Active = mElements[i].GetActive();
                    ri.PartOfView = false;
                    ri.colorIndex = 0;
                    ri.layerIndex = 0;
                    ri.Index = rowIndex++;
                    mRowInformation.push_back(ri);
                }
            }
            else if (mElements[i].GetType()=="timing")
            {
                Row_Information_Struct ri;
                ri.element = &mElements[i];
                ri.Collapsed = mElements[i].GetCollapsed();
                ri.Active = mElements[i].GetActive();
                ri.PartOfView = false;
                ri.colorIndex = timingColorIndex;
                ri.layerIndex = 0;
                if(mSelectedTimingRow<0)
                {
                    mSelectedTimingRow = ri.Active?rowIndex:-1;
                }

                ri.Index = rowIndex++;
                mRowInformation.push_back(ri);
                timingColorIndex++;

            }
            else        // View
            {
                Row_Information_Struct ri;
                ri.element = &mElements[i];
                ri.Collapsed = mElements[i].GetCollapsed();
                ri.Active = mElements[i].GetActive();
                ri.PartOfView = false;
                ri.colorIndex = 0;
                ri.layerIndex = 0;
                ri.Index = rowIndex++;
                mRowInformation.push_back(ri);
                if(!mElements[i].GetCollapsed())
                {
                    // Add models/effect layers in view
                    wxString models = GetViewModels(mElements[i].GetName());
                    if(models.length()> 0)
                    {
                        wxArrayString model=wxSplit(models,',');
                        for(int m=0;m<model.size();m++)
                        {
                            Element* element = GetElement(model[m]);
                            if(element->GetCollapsed())
                            {
                                for(int j=0;element->GetEffectLayerCount();j++)
                                {
                                    Row_Information_Struct r;
                                    r.element = element;
                                    r.Collapsed = element->GetCollapsed();
                                    r.Active = mElements[i].GetActive();
                                    r.PartOfView = false;
                                    r.colorIndex = 0;
                                    r.layerIndex = j;
                                    r.Index = rowIndex++;
                                    mRowInformation.push_back(r);
                                }
                            }
                            else
                            {
                                Row_Information_Struct r;
                                r.element = element;
                                r.Collapsed = element->GetCollapsed();
                                r.Active = mElements[i].GetActive();
                                r.PartOfView = false;
                                r.colorIndex = 0;
                                r.layerIndex = 0;
                                r.Index = rowIndex++;
                                mRowInformation.push_back(r);
                            }
                        }
                    }
                }
            }
        }
    }
}

void SequenceElements::DeactivateAllTimingElements()
{
    for(int i=0;i<mElements.size();i++)
    {
        if(mElements[i].GetType()=="timing")
        {
            mElements[i].SetActive(false);
        }
    }
}

void SequenceElements::SelectEffectsInRowAndPositionRange(int startRow, int endRow, int startX,int endX, int &FirstSelected)
{
    if(startRow<mRowInformation.size())
    {
        if(endRow>=mRowInformation.size())
        {
            endRow = mRowInformation.size()-1;
        }
        for(int i=startRow;i<=endRow;i++)
        {
            EffectLayer* effectLayer = mRowInformation[i].element->GetEffectLayer(mRowInformation[i].layerIndex);
            effectLayer->SelectEffectsInPositionRange(startX,endX,FirstSelected);
        }
    }
}

Effect* SequenceElements::GetSelectedEffectAtRowAndPosition(int row, int x,int &index, int &selectionType)
{
    EffectLayer* effectLayer = mRowInformation[row].element->GetEffectLayer(mRowInformation[row].layerIndex);

    index = effectLayer->GetEffectIndexThatContainsPosition(x,selectionType);
    if(index<0)
    {
        return nullptr;
    }
    else
    {
        return effectLayer->GetEffect(index);
    }
}



void SequenceElements::UnSelectAllEffects()
{
    for(int i=0;i<mRowInformation.size();i++)
    {
        EffectLayer* effectLayer = mRowInformation[i].element->GetEffectLayer(mRowInformation[i].layerIndex);
        effectLayer->UnSelectAllEffects();
    }
}

// Functions to manage selected ranges
int SequenceElements::GetSelectedRangeCount()
{
    return mSelectedRanges.size();
}

EffectRange* SequenceElements::GetSelectedRange(int index)
{
    return &mSelectedRanges[index];
}

void SequenceElements::AddSelectedRange(EffectRange* range)
{
    mSelectedRanges.push_back(*range);
}

void SequenceElements::DeleteSelectedRange(int index)
{
    if(index < mSelectedRanges.size())
    {
        mSelectedRanges.erase(mSelectedRanges.begin()+index);
    }
}

void SequenceElements::ClearSelectedRanges()
{
    mSelectedRanges.clear();
}

