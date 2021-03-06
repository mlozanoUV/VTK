/*=========================================================================

  Program:   Visualization Toolkit

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkOpenGLPointGaussianMapper.h"

#include "vtkOpenGLHelper.h"

#include "vtkCellArray.h"
#include "vtkFloatArray.h"
#include "vtkHardwareSelector.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkMatrix4x4.h"
#include "vtkOpenGLActor.h"
#include "vtkOpenGLCamera.h"
#include "vtkOpenGLIndexBufferObject.h"
#include "vtkOpenGLPolyDataMapper.h"
#include "vtkOpenGLVertexArrayObject.h"
#include "vtkOpenGLVertexBufferObject.h"
#include "vtkOpenGLVertexBufferObjectGroup.h"
#include "vtkPiecewiseFunction.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkShaderProgram.h"

#include "vtkPointGaussianVS.h"
#include "vtkPolyDataFS.h"

#include "vtk_glew.h"

class vtkOpenGLPointGaussianMapperHelper : public vtkOpenGLPolyDataMapper
{
public:
  static vtkOpenGLPointGaussianMapperHelper* New();
  vtkTypeMacro(vtkOpenGLPointGaussianMapperHelper, vtkOpenGLPolyDataMapper)

  vtkPointGaussianMapper *Owner;

  bool UsingPoints;
  float *OpacityTable; // the table
  double OpacityScale; // used for quick lookups
  double OpacityOffset; // used for quick lookups

  float *ScaleTable; // the table
  double ScaleScale; // used for quick lookups
  double ScaleOffset; // used for quick lookups
  double TriangleScale;

protected:
  vtkOpenGLPointGaussianMapperHelper();
  ~vtkOpenGLPointGaussianMapperHelper() override;

  // Description:
  // Create the basic shaders before replacement
  void GetShaderTemplate(
    std::map<vtkShader::Type, vtkShader *> shaders,
    vtkRenderer *, vtkActor *) override;

  // Description:
  // Perform string replacements on the shader templates
  void ReplaceShaderColor(
    std::map<vtkShader::Type, vtkShader *> shaders,
    vtkRenderer *, vtkActor *) override;
  void ReplaceShaderPositionVC(
    std::map<vtkShader::Type, vtkShader *> shaders,
    vtkRenderer *, vtkActor *) override;

  // Description:
  // Set the shader parameters related to the Camera
  void SetCameraShaderParameters(vtkOpenGLHelper &cellBO, vtkRenderer *ren, vtkActor *act) override;

  // Description:
  // Set the shader parameters related to the actor/mapper
  void SetMapperShaderParameters(vtkOpenGLHelper &cellBO, vtkRenderer *ren, vtkActor *act) override;

  // Description:
  // Does the VBO/IBO need to be rebuilt
  bool GetNeedToRebuildBufferObjects(vtkRenderer *ren, vtkActor *act) override;

  // Description:
  // Update the VBO to contain point based values
  void BuildBufferObjects(vtkRenderer *ren, vtkActor *act) override;

  void RenderPieceDraw(vtkRenderer *ren, vtkActor *act) override;

  // create the table for opacity values
  void BuildOpacityTable();

  // create the table for scale values
  void BuildScaleTable();

  // Description:
  // Does the shader source need to be recomputed
  bool GetNeedToRebuildShaders(vtkOpenGLHelper &cellBO,
    vtkRenderer *ren, vtkActor *act) override;

private:
  vtkOpenGLPointGaussianMapperHelper(const vtkOpenGLPointGaussianMapperHelper&) = delete;
  void operator=(const vtkOpenGLPointGaussianMapperHelper&) = delete;
};

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkOpenGLPointGaussianMapperHelper)

//-----------------------------------------------------------------------------
vtkOpenGLPointGaussianMapperHelper::vtkOpenGLPointGaussianMapperHelper()
{
  this->Owner = nullptr;
  this->OpacityTable = nullptr;
  this->ScaleTable = nullptr;
  this->UsingPoints = false;
  this->OpacityScale = 1.0;
  this->ScaleScale = 1.0;
  this->OpacityOffset = 0.0;
  this->ScaleOffset = 0.0;
  this->TriangleScale = 0.0;
}

//-----------------------------------------------------------------------------
void vtkOpenGLPointGaussianMapperHelper::GetShaderTemplate(
  std::map<vtkShader::Type, vtkShader *> shaders,
  vtkRenderer *ren, vtkActor *actor)
{
  this->Superclass::GetShaderTemplate(shaders,ren,actor);

  if (this->Owner->GetScaleFactor() == 0.0)
  {
    this->UsingPoints = true;
  }
  else
  {
    this->UsingPoints = false;
    // for splats use a special shader than handles the offsets
    shaders[vtkShader::Vertex]->SetSource(vtkPointGaussianVS);
  }

}

void vtkOpenGLPointGaussianMapperHelper::ReplaceShaderPositionVC(
  std::map<vtkShader::Type, vtkShader *> shaders,
  vtkRenderer *ren, vtkActor *actor)
{
  if (!this->UsingPoints)
  {
    std::string VSSource = shaders[vtkShader::Vertex]->GetSource();
    std::string FSSource = shaders[vtkShader::Fragment]->GetSource();

    vtkShaderProgram::Substitute(FSSource,
      "//VTK::PositionVC::Dec",
      "varying vec2 offsetVCVSOutput;");

    vtkShaderProgram::Substitute(VSSource,
      "//VTK::Camera::Dec",
      "uniform mat4 VCDCMatrix;\n"
      "uniform mat4 MCVCMatrix;");

    shaders[vtkShader::Vertex]->SetSource(VSSource);
    shaders[vtkShader::Fragment]->SetSource(FSSource);
  }

  this->Superclass::ReplaceShaderPositionVC(shaders,ren,actor);
}

void vtkOpenGLPointGaussianMapperHelper::ReplaceShaderColor(
  std::map<vtkShader::Type, vtkShader *> shaders,
  vtkRenderer *ren, vtkActor *actor)
{
  if (!this->UsingPoints)
  {
    std::string FSSource = shaders[vtkShader::Fragment]->GetSource();

    if (this->Owner->GetSplatShaderCode() && strcmp(this->Owner->GetSplatShaderCode(), "") != 0)
    {
      vtkShaderProgram::Substitute(FSSource,"//VTK::Color::Impl",
        this->Owner->GetSplatShaderCode(), false);
    }
    else
    {
      vtkShaderProgram::Substitute(FSSource,"//VTK::Color::Impl",
        // compute the eye position and unit direction
        "//VTK::Color::Impl\n"
        "  float dist2 = dot(offsetVCVSOutput.xy,offsetVCVSOutput.xy);\n"
        "  float gaussian = exp(-0.5*dist2);\n"
        "  opacity = opacity*gaussian;"
        //  "  opacity = opacity*0.5;"
        , false);
    }
    shaders[vtkShader::Fragment]->SetSource(FSSource);
  }

  this->Superclass::ReplaceShaderColor(shaders,ren,actor);
  //cerr << shaders[vtkShader::Fragment]->GetSource() << endl;
}

//-----------------------------------------------------------------------------
bool vtkOpenGLPointGaussianMapperHelper::GetNeedToRebuildShaders(
  vtkOpenGLHelper &cellBO, vtkRenderer* ren, vtkActor *actor)
{
  this->LastLightComplexity[&cellBO] = 0;

  vtkHardwareSelector* selector = ren->GetSelector();
  int picking = selector ? selector->GetCurrentPass() : -1;
  if (this->LastSelectionState != picking)
  {
    this->SelectionStateChanged.Modified();
    this->LastSelectionState = picking;
  }

  vtkMTimeType renderPassMTime = this->GetRenderPassStageMTime(actor);

  // has something changed that would require us to recreate the shader?
  // candidates are
  // property modified (representation interpolation and lighting)
  // input modified
  // light complexity changed
  if (cellBO.Program == nullptr ||
      cellBO.ShaderSourceTime < this->GetMTime() ||
      cellBO.ShaderSourceTime < actor->GetMTime() ||
      cellBO.ShaderSourceTime < this->CurrentInput->GetMTime() ||
      cellBO.ShaderSourceTime < this->SelectionStateChanged ||
      cellBO.ShaderSourceTime < renderPassMTime)
  {
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------
vtkOpenGLPointGaussianMapperHelper::~vtkOpenGLPointGaussianMapperHelper()
{
  if (this->OpacityTable)
  {
    delete [] this->OpacityTable;
    this->OpacityTable = nullptr;
  }
  if (this->ScaleTable)
  {
    delete [] this->ScaleTable;
    this->ScaleTable = nullptr;
  }
}

//-----------------------------------------------------------------------------
void vtkOpenGLPointGaussianMapperHelper::SetCameraShaderParameters(vtkOpenGLHelper &cellBO,
                                                    vtkRenderer* ren, vtkActor *actor)
{
  if (this->UsingPoints)
  {
    this->Superclass::SetCameraShaderParameters(cellBO,ren,actor);
  }
  else
  {
    vtkShaderProgram *program = cellBO.Program;

    vtkOpenGLCamera *cam = (vtkOpenGLCamera *)(ren->GetActiveCamera());

    vtkMatrix4x4 *wcdc;
    vtkMatrix4x4 *wcvc;
    vtkMatrix3x3 *norms;
    vtkMatrix4x4 *vcdc;
    cam->GetKeyMatrices(ren,wcvc,norms,vcdc,wcdc);
    program->SetUniformMatrix("VCDCMatrix", vcdc);

    if (!actor->GetIsIdentity())
    {
      vtkMatrix4x4 *mcwc;
      vtkMatrix3x3 *anorms;
      ((vtkOpenGLActor *)actor)->GetKeyMatrices(mcwc,anorms);
      vtkMatrix4x4::Multiply4x4(mcwc, wcvc, this->TempMatrix4);
      program->SetUniformMatrix("MCVCMatrix", this->TempMatrix4);
    }
    else
    {
      program->SetUniformMatrix("MCVCMatrix", wcvc);
    }

    // add in uniforms for parallel and distance
    cellBO.Program->SetUniformi("cameraParallel", cam->GetParallelProjection());
  }
}

//-----------------------------------------------------------------------------
void vtkOpenGLPointGaussianMapperHelper::SetMapperShaderParameters(
  vtkOpenGLHelper &cellBO,
  vtkRenderer *ren, vtkActor *actor)
{
  if (!this->UsingPoints)
  {
    cellBO.Program->SetUniformf("triangleScale",this->TriangleScale);
  }
  this->Superclass::SetMapperShaderParameters(cellBO,ren,actor);
}

namespace
{

template< typename PointDataType >
PointDataType vtkOpenGLPointGaussianMapperHelperGetComponent(
  PointDataType* tuple,
  int nComponent,
  int component
)
{
  // If this is a single component array, make sure we do not compute
  // a useless magnitude
  if (nComponent == 1)
  {
    component = 0;
  }

  // If we request a non-existing componeent, return the magnitude of the tuple
  PointDataType compVal = 0.0;
  if (component < 0 || component >= nComponent)
  {
    for (int iComp = 0; iComp < nComponent; iComp++)
    {
      PointDataType tmp = tuple[iComp];
      compVal += tmp * tmp;
    }
    compVal = sqrt(compVal);
  }
  else
  {
    compVal = tuple[component];
  }
  return compVal;
}

void vtkOpenGLPointGaussianMapperHelperComputeColor(
  unsigned char* rcolor,
  unsigned char *colors,
  int colorComponents,
  vtkIdType index,
  vtkDataArray *opacities,
  int opacitiesComponent,
  vtkOpenGLPointGaussianMapperHelper *self
)
{
  unsigned char white[4] = {255, 255, 255, 255};

  // if there are no per point sizes and the default size is zero
  // then just render points, saving memory and speed
  unsigned char *colorPtr = colors ? (colors + index*colorComponents) : white;
  rcolor[0] = *(colorPtr++);
  rcolor[1] = *(colorPtr++);
  rcolor[2] = *(colorPtr++);

  if (opacities)
  {
    double opacity = vtkOpenGLPointGaussianMapperHelperGetComponent<double>(
      opacities->GetTuple(index), opacities->GetNumberOfComponents(), opacitiesComponent);
    if (self->OpacityTable)
    {
      double tindex = (opacity - self->OpacityOffset)*self->OpacityScale;
      int itindex = static_cast<int>(tindex);
      if (itindex >= self->Owner->GetOpacityTableSize() - 1)
      {
        opacity = self->OpacityTable[self->Owner->GetOpacityTableSize() - 1];
      }
      else if (itindex < 0)
      {
        opacity = self->OpacityTable[0];
      }
      else
      {
        opacity = (1.0 - tindex + itindex)*self->OpacityTable[itindex] + (tindex - itindex)*self->OpacityTable[itindex+1];
      }
    }
    rcolor[3] = static_cast<float>(opacity*255.0);
  }
  else
  {
    rcolor[3] = (colorComponents == 4 ? *colorPtr : 255);
  }
}

void vtkOpenGLPointGaussianMapperHelperColors(
    vtkUnsignedCharArray *outColors,
    vtkIdType numPts,
    unsigned char *colors, int colorComponents,
    vtkDataArray *opacities,
    int opacitiesComponent,
    vtkOpenGLPointGaussianMapperHelper *self,
    vtkCellArray *verts)
{
  unsigned char *vPtr = static_cast<unsigned char *>(
    outColors->GetVoidPointer(0));

  // iterate over cells or not
  if (verts->GetNumberOfCells())
  {
    vtkIdType* indices(nullptr);
    vtkIdType npts(0);
    for (verts->InitTraversal(); verts->GetNextCell(npts, indices); )
    {
      for (int i = 0; i < npts; ++i)
      {
        vtkOpenGLPointGaussianMapperHelperComputeColor(
          vPtr, colors, colorComponents, indices[i], opacities,
          opacitiesComponent, self);
        if (!self->UsingPoints)
        {
          memcpy(vPtr + 4, vPtr, 4);
          memcpy(vPtr + 8, vPtr, 4);
          vPtr += 8;
        }
        vPtr += 4;
      }
    }
  }
  else
  {
    for (vtkIdType i = 0; i < numPts; i++)
    {
      vtkOpenGLPointGaussianMapperHelperComputeColor(
        vPtr, colors, colorComponents, i, opacities,
        opacitiesComponent, self);
      if (!self->UsingPoints)
      {
        memcpy(vPtr + 4, vPtr, 4);
        memcpy(vPtr + 8, vPtr, 4);
        vPtr += 8;
      }
      vPtr += 4;
    }
  }
}

float vtkOpenGLPointGaussianMapperHelperGetRadius(
  double radius,
  vtkOpenGLPointGaussianMapperHelper *self
)
{
  if (self->ScaleTable)
  {
    double tindex = (radius - self->ScaleOffset)*self->ScaleScale;
    int itindex = static_cast<int>(tindex);
    if (itindex >= self->Owner->GetScaleTableSize() - 1)
    {
      radius = self->ScaleTable[self->Owner->GetScaleTableSize() - 1];
    }
    else if (itindex < 0)
    {
      radius = self->ScaleTable[0];
    }
    else
    {
      radius = (1.0 - tindex + itindex)*self->ScaleTable[itindex] + (tindex - itindex)*self->ScaleTable[itindex+1];
    }
  }
  radius *= self->Owner->GetScaleFactor();
  radius *= self->TriangleScale;

  return static_cast<float>(radius);
}

template< typename PointDataType >
void vtkOpenGLPointGaussianMapperHelperSizes(
    vtkFloatArray *scales,
    PointDataType* sizes,
    int nComponent,
    int component,
    vtkIdType numPts,
    vtkOpenGLPointGaussianMapperHelper *self,
    vtkCellArray *verts)
{
  float *it = static_cast<float *>(scales->GetVoidPointer(0));
  float cos30 = cos(vtkMath::RadiansFromDegrees(30.0));

  // iterate over cells or not
  if (verts->GetNumberOfCells())
  {
    vtkIdType* indices(nullptr);
    vtkIdType npts(0);
    for (verts->InitTraversal(); verts->GetNextCell(npts, indices); )
    {
      for (vtkIdType i = 0; i < npts; ++i)
      {
        PointDataType size = 1.0;
        if (sizes)
        {
          size = vtkOpenGLPointGaussianMapperHelperGetComponent<PointDataType>(
            &sizes[indices[i] * nComponent], nComponent, component);
        }
        float radiusFloat =
          vtkOpenGLPointGaussianMapperHelperGetRadius(size, self);
        *(it++) = -2.0f*radiusFloat*cos30;
        *(it++) = -radiusFloat;
        *(it++) = 2.0f*radiusFloat*cos30;
        *(it++) = -radiusFloat;
        *(it++) = 0.0f;
        *(it++) = 2.0f*radiusFloat;
      }
    }
  }
  else
  {
    for (vtkIdType i = 0; i < numPts; i++)
    {
      PointDataType size = 1.0;
      if (sizes)
      {
        size = vtkOpenGLPointGaussianMapperHelperGetComponent<PointDataType>(
          &sizes[i * nComponent], nComponent, component);
      }
      float radiusFloat =
        vtkOpenGLPointGaussianMapperHelperGetRadius(size, self);
      *(it++) = -2.0f*radiusFloat*cos30;
      *(it++) = -radiusFloat;
      *(it++) = 2.0f*radiusFloat*cos30;
      *(it++) = -radiusFloat;
      *(it++) = 0.0f;
      *(it++) = 2.0f*radiusFloat;
    }
  }
}

template< typename PointDataType >
void vtkOpenGLPointGaussianMapperHelperPoints(
    vtkFloatArray *vcoords,
    PointDataType* points, vtkIdType numPts,
    vtkOpenGLPointGaussianMapperHelper *self,
    vtkCellArray *verts)
{
  float *vPtr = static_cast<float *>(vcoords->GetVoidPointer(0));
  PointDataType *pointPtr;

  // iterate over cells or not
  if (verts->GetNumberOfCells())
  {
    vtkIdType* indices(nullptr);
    vtkIdType npts(0);
    for (verts->InitTraversal(); verts->GetNextCell(npts, indices); )
    {
      for (int i = 0; i < npts; ++i)
      {
        pointPtr = points + indices[i]*3;

        // Vertices
        *(vPtr++) = pointPtr[0];
        *(vPtr++) = pointPtr[1];
        *(vPtr++) = pointPtr[2];
        if (!self->UsingPoints)
        {
          *(vPtr++) = pointPtr[0];
          *(vPtr++) = pointPtr[1];
          *(vPtr++) = pointPtr[2];
          *(vPtr++) = pointPtr[0];
          *(vPtr++) = pointPtr[1];
          *(vPtr++) = pointPtr[2];
        }
      }
    }
  }
  else
  {
    for (vtkIdType i = 0; i < numPts; i++)
    {
      pointPtr = points + i*3;

      // Vertices
      *(vPtr++) = pointPtr[0];
      *(vPtr++) = pointPtr[1];
      *(vPtr++) = pointPtr[2];
      if (!self->UsingPoints)
      {
        *(vPtr++) = pointPtr[0];
        *(vPtr++) = pointPtr[1];
        *(vPtr++) = pointPtr[2];
        *(vPtr++) = pointPtr[0];
        *(vPtr++) = pointPtr[1];
        *(vPtr++) = pointPtr[2];
      }
    }
  }
}

} // anonymous namespace

//-------------------------------------------------------------------------
bool vtkOpenGLPointGaussianMapperHelper::GetNeedToRebuildBufferObjects(
  vtkRenderer *vtkNotUsed(ren),
  vtkActor *act)
{
  // picking state does not require a rebuild, unlike our parent
  if (this->VBOBuildTime < this->GetMTime() ||
      this->VBOBuildTime < act->GetMTime() ||
      this->VBOBuildTime < this->CurrentInput->GetMTime() ||
      this->VBOBuildTime < this->Owner->GetMTime() ||
      (this->Owner->GetScalarOpacityFunction() &&
        this->VBOBuildTime < this->Owner->GetScalarOpacityFunction()->GetMTime()) ||
      (this->Owner->GetScaleFunction() &&
        this->VBOBuildTime < this->Owner->GetScaleFunction()->GetMTime())
      )
  {
    return true;
  }
  return false;
}

//-------------------------------------------------------------------------
void vtkOpenGLPointGaussianMapperHelper::BuildOpacityTable()
{
  double range[2];

  // if a piecewise function was provided, use it to map the opacities
  vtkPiecewiseFunction *pwf = this->Owner->GetScalarOpacityFunction();
  int tableSize = this->Owner->GetOpacityTableSize();

  delete [] this->OpacityTable;
  this->OpacityTable = new float [tableSize+1];
  if (pwf)
  {
    // build the interpolation table
    pwf->GetRange(range);
    pwf->GetTable(range[0],range[1],tableSize,this->OpacityTable);
    // duplicate the last value for bilinear interp edge case
    this->OpacityTable[tableSize] = this->OpacityTable[tableSize-1];
    this->OpacityScale = (tableSize - 1.0)/(range[1] - range[0]);
    this->OpacityOffset = range[0];
  }

}

//-------------------------------------------------------------------------
void vtkOpenGLPointGaussianMapperHelper::BuildScaleTable()
{
  double range[2];

  // if a piecewise function was provided, use it to map the opacities
  vtkPiecewiseFunction *pwf = this->Owner->GetScaleFunction();
  int tableSize = this->Owner->GetScaleTableSize();

  delete [] this->ScaleTable;
  this->ScaleTable = new float [tableSize+1];
  if (pwf)
  {
    // build the interpolation table
    pwf->GetRange(range);
    pwf->GetTable(range[0],range[1],tableSize,this->ScaleTable);
    // duplicate the last value for bilinear interp edge case
    this->ScaleTable[tableSize] = this->ScaleTable[tableSize-1];
    this->ScaleScale = (tableSize - 1.0)/(range[1] - range[0]);
    this->ScaleOffset = range[0];
  }
}

//-------------------------------------------------------------------------
void vtkOpenGLPointGaussianMapperHelper::BuildBufferObjects(
  vtkRenderer *ren, vtkActor *vtkNotUsed(act))
{
  vtkPolyData *poly = this->CurrentInput;

  if (poly == nullptr)
  {
    return;
  }

  // set the triangle scale
  this->TriangleScale = this->Owner->GetTriangleScale();

  bool hasScaleArray = this->Owner->GetScaleArray() != nullptr &&
    poly->GetPointData()->HasArray(this->Owner->GetScaleArray());
  if (hasScaleArray && this->Owner->GetScaleFunction())
  {
    this->BuildScaleTable();
  }
  else
  {
    if (this->ScaleTable)
    {
      delete [] this->ScaleTable;
      this->ScaleTable = nullptr;
    }
  }

  if (this->Owner->GetScaleFactor() == 0.0)
  {
    this->UsingPoints = true;
  }
  else
  {
    this->UsingPoints = false;
  }

  // if we have an opacity array then get it and if we have
  // a ScalarOpacityFunction map the array through it
  bool hasOpacityArray = this->Owner->GetOpacityArray() != nullptr &&
    poly->GetPointData()->HasArray(this->Owner->GetOpacityArray());
  if (hasOpacityArray && this->Owner->GetScalarOpacityFunction())
  {
    this->BuildOpacityTable();
  }
  else
  {
    if (this->OpacityTable)
    {
      delete [] this->OpacityTable;
      this->OpacityTable = nullptr;
    }
  }

  // For vertex coloring, this sets this->Colors as side effect.
  // For texture map coloring, this sets ColorCoordinates
  // and ColorTextureMap as a side effect.
  // I moved this out of the conditional because it is fast.
  // Color arrays are cached. If nothing has changed,
  // then the scalars do not have to be regenerted.
  this->MapScalars(1.0);

  // Figure out how big each block will be, currently 6 floats.

  int splatCount = poly->GetPoints()->GetNumberOfPoints();
  if (poly->GetVerts()->GetNumberOfCells())
  {
    splatCount = poly->GetVerts()->GetNumberOfConnectivityEntries() -
      poly->GetVerts()->GetNumberOfCells();
  }
  if (!this->UsingPoints)
  {
    splatCount *= 3;
  }

  vtkFloatArray *pts = vtkFloatArray::New();
  pts->SetNumberOfComponents(3);
  pts->SetNumberOfTuples(splatCount);
  switch(poly->GetPoints()->GetDataType())
  {
    vtkTemplateMacro(
      vtkOpenGLPointGaussianMapperHelperPoints(
        pts,
        static_cast<VTK_TT*>(poly->GetPoints()->GetVoidPointer(0)),
        poly->GetPoints()->GetNumberOfPoints(),
        this,
        poly->GetVerts()
        ));
  }
  this->VBOs->CacheDataArray("vertexMC", pts, ren, VTK_FLOAT);
  pts->Delete();

  if (!this->UsingPoints)
  {
    vtkFloatArray *offsets = vtkFloatArray::New();
    offsets->SetNumberOfComponents(2);
    offsets->SetNumberOfTuples(splatCount);

    if (hasScaleArray)
    {
      vtkDataArray *sizes = poly->GetPointData()->GetArray(
              this->Owner->GetScaleArray());
      switch (sizes->GetDataType())
        {
          vtkTemplateMacro(
              vtkOpenGLPointGaussianMapperHelperSizes(
                offsets,
                static_cast<VTK_TT*>(sizes->GetVoidPointer(0)),
                sizes->GetNumberOfComponents(),
                this->Owner->GetScaleArrayComponent(),
                poly->GetPoints()->GetNumberOfPoints(),
                this,
                poly->GetVerts()
              ));
        }
      }
      else
      {
        vtkOpenGLPointGaussianMapperHelperSizes(
          offsets,
          static_cast<float *>(nullptr), 0, 0,
          poly->GetPoints()->GetNumberOfPoints(),
          this,
          poly->GetVerts()
        );
      }
      this->VBOs->CacheDataArray("offsetMC", offsets, ren, VTK_FLOAT);
      offsets->Delete();
    }
    else
    {
      this->VBOs->CacheDataArray("offsetMC", nullptr, ren, VTK_FLOAT);
    }

    if (this->Colors)
    {
      vtkUnsignedCharArray *clrs = vtkUnsignedCharArray::New();
      clrs->SetNumberOfComponents(4);
      clrs->SetNumberOfTuples(splatCount);

      vtkOpenGLPointGaussianMapperHelperColors(
        clrs,
        poly->GetPoints()->GetNumberOfPoints(),
        this->Colors ? (unsigned char *)this->Colors->GetVoidPointer(0) : (unsigned char*)nullptr,
        this->Colors ? this->Colors->GetNumberOfComponents() : 0,
        hasOpacityArray ? poly->GetPointData()->GetArray(
          this->Owner->GetOpacityArray()) : (vtkDataArray*)nullptr,
        this->Owner->GetOpacityArrayComponent(), this, poly->GetVerts()
        );
      this->VBOs->CacheDataArray("scalarColor", clrs, ren, VTK_UNSIGNED_CHAR);
      clrs->Delete();
      }

    this->VBOs->BuildAllVBOs(ren);

    // we use no IBO
    for (int i = PrimitiveStart; i < PrimitiveEnd; i++)
    {
      this->Primitives[i].IBO->IndexCount = 0;
    }
    this->Primitives[PrimitiveTris].IBO->IndexCount = splatCount;
    this->VBOBuildTime.Modified();
}

//-----------------------------------------------------------------------------
void vtkOpenGLPointGaussianMapperHelper::RenderPieceDraw(vtkRenderer* ren, vtkActor *actor)
{
  // draw polygons
  int numVerts = this->VBOs->GetNumberOfTuples("vertexMC");
  if (numVerts)
  {
    // save off current state of src / dst blend functions
    GLint blendSrcA = GL_SRC_ALPHA;
    GLint blendDstA = GL_ONE;
    GLint blendSrcC = GL_SRC_ALPHA;
    GLint blendDstC = GL_ONE;
    if (this->Owner->GetEmissive() != 0)
    {
      glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcA);
      glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstA);
      glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcC);
      glGetIntegerv(GL_BLEND_DST_RGB, &blendDstC);
      glBlendFunc( GL_SRC_ALPHA, GL_ONE);  // additive for emissive sources
    }
    // First we do the triangles or points, update the shader, set uniforms, etc.
    this->UpdateShaders(this->Primitives[PrimitiveTris], ren, actor);
    if (this->UsingPoints)
    {
      glDrawArrays(GL_POINTS, 0,
        static_cast<GLuint>(numVerts));
    }
    else
    {
      glDrawArrays(GL_TRIANGLES, 0,
        static_cast<GLuint>(numVerts));
    }
    if (this->Owner->GetEmissive() != 0)
    {
      // restore blend func
      glBlendFuncSeparate(blendSrcC, blendDstC, blendSrcA, blendDstA);
    }
  }
}

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkOpenGLPointGaussianMapper)

//-----------------------------------------------------------------------------
vtkOpenGLPointGaussianMapper::vtkOpenGLPointGaussianMapper()
{
  this->Helper = vtkOpenGLPointGaussianMapperHelper::New();
  this->Helper->Owner = this;
}

vtkOpenGLPointGaussianMapper::~vtkOpenGLPointGaussianMapper()
{
  this->Helper->Delete();
  this->Helper = nullptr;
}

void vtkOpenGLPointGaussianMapper::RenderPiece(vtkRenderer *ren, vtkActor *act)
{
  if (this->GetMTime() > this->HelperUpdateTime)
  {
    this->Helper->vtkPolyDataMapper::ShallowCopy(this);
    this->Helper->Modified();
    this->HelperUpdateTime.Modified();
  }
  this->Helper->RenderPiece(ren,act);
}

//-----------------------------------------------------------------------------
void vtkOpenGLPointGaussianMapper::ReleaseGraphicsResources(vtkWindow* win)
{
  this->Helper->ReleaseGraphicsResources(win);
  this->Helper->SetInputData(nullptr);
  this->Modified();
}

//-----------------------------------------------------------------------------
bool vtkOpenGLPointGaussianMapper::GetIsOpaque()
{
  if (this->Emissive)
  {
    return false;
  }
  return this->Superclass::GetIsOpaque();
}

//-----------------------------------------------------------------------------
void vtkOpenGLPointGaussianMapper::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
