# create camera figure

source vtkInt.tcl

# create the render master
vtkRenderMaster rm;

# Now create the RenderWindow, Renderer and both Actors
#
set renWin [rm MakeRenderWindow];
set ren1   [$renWin MakeRenderer];
set iren [$renWin MakeRenderWindowInteractor];

# create a camera model
vtkConeSource camCS;
camCS SetHeight 1.5;
camCS SetResolution 12;
camCS SetRadius 0.4;

vtkCubeSource camCBS;
camCBS SetXLength 1.5;
camCBS SetZLength 0.8;
camCBS SetCenter 0.4 0 0;

vtkAppendPolyData camAPD;
camAPD AddInput [camCBS GetOutput];
camAPD AddInput [camCS GetOutput];

vtkPolyMapper camMapper;
    camMapper SetInput [camAPD GetOutput];
vtkLODActor camActor;
    camActor SetMapper camMapper;
camActor SetScale 2 2 2;
camActor SetPosition 3 0 0;

# draw the arrows
vtkPolyData pd;
vtkCellArray ca;
vtkFloatPoints fp;
fp InsertNextPoint 0 1 0;
fp InsertNextPoint 8 1 0;
fp InsertNextPoint 8 2 0;
fp InsertNextPoint 10 0 0;
fp InsertNextPoint 8 -2 0;
fp InsertNextPoint 8 -1 0;
fp InsertNextPoint 0 -1 0;
ca InsertNextCell 7;
ca InsertCellPoint 0;
ca InsertCellPoint 1;
ca InsertCellPoint 2;
ca InsertCellPoint 3;
ca InsertCellPoint 4;
ca InsertCellPoint 5;
ca InsertCellPoint 6;
pd SetPoints fp;
pd SetPolys ca;

vtkImplicitModeller arrowIM;
arrowIM SetInput pd;
arrowIM SetSampleDimensions 50 20 8;

vtkContourFilter arrowCF;
arrowCF SetInput [arrowIM GetOutput];
arrowCF SetValue 0 0.2;

vtkWarpTo arrowWT;
arrowWT SetInput [arrowCF GetOutput];
arrowWT SetPosition 5 0 5;
arrowWT SetScaleFactor 0.85;
arrowWT AbsoluteOn;

vtkTransform arrowT;
arrowT Scale 1 0.5 1;
arrowT RotateY -20;
arrowT Translate -3.63576 0 -3.69263;

vtkTransformFilter arrowTF;
arrowTF SetInput [arrowWT GetOutput];
arrowTF SetTransform arrowT;

vtkDataSetMapper arrowMapper;
arrowMapper SetInput [arrowTF GetOutput];
arrowMapper ScalarsVisibleOff;

# draw the azimuth arrows
vtkLODActor a1Actor;
a1Actor SetMapper arrowMapper;
a1Actor RotateY -90;
a1Actor SetPosition 1 0 3;
[a1Actor GetProperty] SetColor 1 0.3 0.3;
[a1Actor GetProperty] SetSpecularColor 1 1 1;
[a1Actor GetProperty] SetSpecular 0.3;
[a1Actor GetProperty] SetSpecularPower 20;
[a1Actor GetProperty] SetAmbient 0.2;
[a1Actor GetProperty] SetDiffuse 0.8;

vtkLODActor a2Actor;
a2Actor SetMapper arrowMapper;
a2Actor RotateX 180;
a2Actor RotateY -90;
a2Actor SetPosition 1 0 -3;
[a2Actor GetProperty] SetColor 1 0.3 0.3;
[a2Actor GetProperty] SetSpecularColor 1 1 1;
[a2Actor GetProperty] SetSpecular 0.3;
[a2Actor GetProperty] SetSpecularPower 20;
[a2Actor GetProperty] SetAmbient 0.2;
[a2Actor GetProperty] SetDiffuse 0.8;

# draw the elevation arrows
vtkLODActor a3Actor;
a3Actor SetMapper arrowMapper;
a3Actor RotateX 270;
a3Actor RotateY -90;
a3Actor SetPosition 1 3 0;
[a3Actor GetProperty] SetColor 0.3 1 0.3;
[a3Actor GetProperty] SetSpecularColor 1 1 1;
[a3Actor GetProperty] SetSpecular 0.3;
[a3Actor GetProperty] SetSpecularPower 20;
[a3Actor GetProperty] SetAmbient 0.2;
[a3Actor GetProperty] SetDiffuse 0.8;

vtkLODActor a4Actor;
a4Actor SetMapper arrowMapper;
a4Actor RotateX 90;
a4Actor RotateY -90;
a4Actor SetPosition 1 -3 0;
[a4Actor GetProperty] SetColor 0.3 1 0.3;
[a4Actor GetProperty] SetSpecularColor 1 1 1;
[a4Actor GetProperty] SetSpecular 0.3;
[a4Actor GetProperty] SetSpecularPower 20;
[a4Actor GetProperty] SetAmbient 0.2;
[a4Actor GetProperty] SetDiffuse 0.8;

# draw the DOP
vtkTransform arrowT2;
arrowT2 Scale 1 0.6 1;
arrowT2 RotateY 90;

vtkTransformPolyFilter arrowTF2;
arrowTF2 SetInput pd;
arrowTF2 SetTransform arrowT2;

vtkRotationalExtrusionFilter arrowREF;
arrowREF SetInput [arrowTF2 GetOutput];
arrowREF SetAngle 390;
arrowREF CappingOff;

vtkPolyMapper spikeMapper;
spikeMapper SetInput [arrowREF GetOutput];

vtkLODActor a5Actor;
a5Actor SetMapper spikeMapper;
a5Actor SetScale .3 .3 .6;
a5Actor RotateY 90;
a5Actor SetPosition -1 0 0;
[a5Actor GetProperty] SetColor 1 0.3 1;
[a5Actor GetProperty] SetSpecularColor 1 1 1;
[a5Actor GetProperty] SetSpecular 0.3;
[a5Actor GetProperty] SetAmbient 0.2;
[a5Actor GetProperty] SetDiffuse 0.8;
[a5Actor GetProperty] SetSpecularPower 20;

# focal point
vtkSphereSource fps;
fps SetRadius 0.4;
vtkPolyMapper fpMapper;
fpMapper SetInput [fps GetOutput];
vtkLODActor fpActor;
fpActor SetMapper fpMapper;
fpActor SetPosition -6 0 0;
[fpActor GetProperty] SetSpecularColor 1 1 1;
[fpActor GetProperty] SetSpecular 0.3;
[fpActor GetProperty] SetAmbient 0.2;
[fpActor GetProperty] SetDiffuse 0.8;
[fpActor GetProperty] SetSpecularPower 20;

# create the roll arrows
vtkWarpTo arrowWT2;
arrowWT2 SetInput [arrowCF GetOutput];
arrowWT2 SetPosition 5 0 2.5;
arrowWT2 SetScaleFactor 0.95;
arrowWT2 AbsoluteOn;

vtkTransform arrowT3;
arrowT3 Translate -2.50358 0 -1.70408;
arrowT3 Scale 0.5 0.3 1;

vtkTransformFilter arrowTF3;
arrowTF3 SetInput [arrowWT2 GetOutput];
arrowTF3 SetTransform arrowT3;

vtkDataSetMapper arrowMapper2;
arrowMapper2 SetInput [arrowTF3 GetOutput];
arrowMapper2 ScalarsVisibleOff;

# draw the roll arrows
vtkLODActor a6Actor;
a6Actor SetMapper arrowMapper2;
a6Actor RotateZ 90;
a6Actor SetPosition -2 0 0;
a6Actor SetScale 1.5 1.5 1.5;
[a6Actor GetProperty] SetColor 1 1 0.3;
[a6Actor GetProperty] SetSpecularColor 1 1 1;
[a6Actor GetProperty] SetSpecular 0.3;
[a6Actor GetProperty] SetSpecularPower 20;
[a6Actor GetProperty] SetAmbient 0.2;
[a6Actor GetProperty] SetDiffuse 0.8;

# Add the actors to the renderer, set the background and size
$ren1 AddActors camActor;
$ren1 AddActors a1Actor;
$ren1 AddActors a2Actor;
$ren1 AddActors a3Actor;
$ren1 AddActors a4Actor;
$ren1 AddActors a5Actor;
$ren1 AddActors a6Actor;
$ren1 AddActors fpActor;
$ren1 SetBackground 0.1 0.2 0.4;
$renWin SetSize 500 500;

# render the image
$iren SetUserMethod {wm deiconify .vtkInteract};
$iren Initialize;
set cam1 [$ren1 GetActiveCamera];

$renWin SetFilename camera1.ppm;

# prevent the tk window from showing up then start the event loop
wm withdraw .
$iren Start;


