{ UpdateLCSC.pas - Cambia LCSC_PN (y opcionalmente Comment y Manufacturer_Part_Number)
  en los componentes del esquemático del proyecto enfocado.

  Uso (Altium Designer 24):
    1. Abre IncuNest.PrjPcb (el script lo busca por ruta entre los proyectos abiertos).
    2. File > Run Script... > Browse > tools\altium\UpdateLCSC.pas > procedimiento UpdateLCSC.
    3. Revisa el mensaje final, guarda los SchDoc (File > Save All).
    4. Design > Update PCB Document Motherboard.PcbDoc (ECO) para pasar los parámetros a la PCB.
    5. Regenera el Output Job y comprueba con: python tools/sync.py y python tools/stock.py 50

  Formato de cada cambio: Designador | LCSC_PN | Comment (vacío = no tocar) | MPN (vacío = no tocar)
}

Var
    Changes : TStringList;
    Done    : TStringList;

Procedure AddChange(Des, Lcsc, Cmt, Mpn : String);
Begin
    Changes.Add(Des + '|' + Lcsc + '|' + Cmt + '|' + Mpn);
End;

Procedure LoadChanges;
Begin
    // Revisión del 22/10/2026 (R70, D8 y el LCSC_PN de R46 ya estaban cambiados a mano)
    // Shunts: Comment y MPN reales de C728276 (Yageo PA1206 5 mOhm 1 W); R46 seguía como 0.002R
    AddChange('R46', 'C844294',  '0.002R 1206 1W 1%', 'WSLP12062L000FEA');   // 2 mOhm (22/10): menos disipación en el calefactor
    AddChange('R17', 'C728276',  '0.005R 1206 1W 1%', 'PA1206FRE470R005Z');
    AddChange('R48', 'C728276',  '0.005R 1206 1W 1%', 'PA1206FRE470R005Z');
    AddChange('R49', 'C844294',  '0.002R 1206 1W 1%', 'WSLP12062L000FEA');   // 2 mOhm (25/10): 12 A de V_SYS
    AddChange('R59', 'C728276',  '0.005R 1206 1W 1%', 'PA1206FRE470R005Z');
    // U11 perdió el parámetro LCSC_PN (estaba en la exportación del 21/10); sin él JLC no casa la pieza de tu librería
    AddChange('U11', 'C5219071', '',                  '');
End;

Function Field(S : String; Idx : Integer) : String;
Var
    i, p : Integer;
Begin
    For i := 1 To Idx Do
    Begin
        p := Pos('|', S);
        S := Copy(S, p + 1, Length(S));
    End;
    p := Pos('|', S);
    If p > 0 Then
        Result := Copy(S, 1, p - 1)
    Else
        Result := S;
End;

Function FindChange(Des : String) : Integer;
Var
    i : Integer;
Begin
    Result := -1;
    For i := 0 To Changes.Count - 1 Do
        If UpperCase(Field(Changes[i], 0)) = UpperCase(Des) Then
            Result := i;
End;

Function SetParamIfExists(Comp : ISch_Component; PName, PValue : String) : Boolean;
Var
    It    : ISch_Iterator;
    Param : ISch_Parameter;
Begin
    Result := False;
    It := Comp.SchIterator_Create;
    It.AddFilter_ObjectSet(MkSet(eParameter));
    Param := It.FirstSchObject;
    While Param <> Nil Do
    Begin
        If UpperCase(Param.Name) = UpperCase(PName) Then
        Begin
            SchServer.RobotManager.SendMessage(Param.I_ObjectAddress, c_BroadCast, SCHM_BeginModify, c_NoEventData);
            Param.Text := PValue;
            SchServer.RobotManager.SendMessage(Param.I_ObjectAddress, c_BroadCast, SCHM_EndModify, c_NoEventData);
            Result := True;
        End;
        Param := It.NextSchObject;
    End;
    Comp.SchIterator_Destroy(It);
End;

Procedure SetParam(Comp : ISch_Component; PName, PValue : String);
Var
    It    : ISch_Iterator;
    Param : ISch_Parameter;
    Found : Boolean;
Begin
    If PValue = '' Then Exit;
    Found := False;
    It := Comp.SchIterator_Create;
    It.AddFilter_ObjectSet(MkSet(eParameter));
    Param := It.FirstSchObject;
    While Param <> Nil Do
    Begin
        If UpperCase(Param.Name) = UpperCase(PName) Then
        Begin
            SchServer.RobotManager.SendMessage(Param.I_ObjectAddress, c_BroadCast, SCHM_BeginModify, c_NoEventData);
            Param.Text := PValue;
            SchServer.RobotManager.SendMessage(Param.I_ObjectAddress, c_BroadCast, SCHM_EndModify, c_NoEventData);
            Found := True;
        End;
        Param := It.NextSchObject;
    End;
    Comp.SchIterator_Destroy(It);

    If Not Found Then
    Begin
        Param := SchServer.SchObjectFactory(eParameter, eCreate_Default);
        Param.Name := PName;
        Param.Text := PValue;
        Param.IsHidden := True;
        Param.Location := Comp.Location;
        Comp.AddSchObject(Param);
        SchServer.RobotManager.SendMessage(Comp.I_ObjectAddress, c_BroadCast, SCHM_PrimitiveRegistration, Param.I_ObjectAddress);
    End;
End;

// El MPN aparece como 'Manufacturer Part Number' o 'Manufacturer_Part_Number' según la librería de origen
Procedure SetMPN(Comp : ISch_Component; PValue : String);
Var
    A, B : Boolean;
Begin
    If PValue = '' Then Exit;
    A := SetParamIfExists(Comp, 'Manufacturer Part Number', PValue);
    B := SetParamIfExists(Comp, 'Manufacturer_Part_Number', PValue);
    If Not (A Or B) Then
        SetParam(Comp, 'Manufacturer Part Number', PValue);
End;

Procedure ProcessSheet(SchDoc : ISch_Document);
Var
    It   : ISch_Iterator;
    Comp : ISch_Component;
    k    : Integer;
    S    : String;
Begin
    It := SchDoc.SchIterator_Create;
    It.AddFilter_ObjectSet(MkSet(eSchComponent));
    Comp := It.FirstSchObject;
    While Comp <> Nil Do
    Begin
        k := FindChange(Comp.Designator.Text);
        If k >= 0 Then
        Begin
            S := Changes[k];
            SchServer.RobotManager.SendMessage(Comp.I_ObjectAddress, c_BroadCast, SCHM_BeginModify, c_NoEventData);
            SetParam(Comp, 'LCSC_PN', Field(S, 1));
            If Field(S, 2) <> '' Then
                Comp.Comment.Text := Field(S, 2);
            SetMPN(Comp, Field(S, 3));
            SchServer.RobotManager.SendMessage(Comp.I_ObjectAddress, c_BroadCast, SCHM_EndModify, c_NoEventData);
            Done.Add(Comp.Designator.Text + ' -> ' + Field(S, 1) + '  (' + ExtractFileName(SchDoc.DocumentName) + ')');
        End;
        Comp := It.NextSchObject;
    End;
    SchDoc.SchIterator_Destroy(It);
    SchDoc.GraphicallyInvalidate;
End;

Procedure UpdateLCSC;
Var
    Project : IProject;
    Doc     : IDocument;
    SchDoc  : ISch_Document;
    SrvDoc  : IServerDocument;
    i, j    : Integer;
    Missing : String;
Begin
    Changes := TStringList.Create;
    Done := TStringList.Create;
    LoadChanges;

    // proyecto IncuNest.PrjPcb de la Motherboard (no el enfocado: puede haber otros proyectos abiertos)
    Project := Nil;
    For i := 0 To GetWorkspace.DM_ProjectCount - 1 Do
        If Pos('MOTHERBOARD\INCUNEST.PRJPCB', UpperCase(GetWorkspace.DM_Projects(i).DM_ProjectFullPath)) > 0 Then
            Project := GetWorkspace.DM_Projects(i);
    If Project = Nil Then
        Project := GetWorkspace.DM_OpenProject('C:\Users\Pablo\Documents\IncuNest\Hardware\Electronics\Motherboard\IncuNest.PrjPcb', True);
    If Project = Nil Then
    Begin
        ShowMessage('No encuentro Motherboard\IncuNest.PrjPcb abierto en el workspace.');
        Exit;
    End;

    For i := 0 To Project.DM_LogicalDocumentCount - 1 Do
    Begin
        Doc := Project.DM_LogicalDocuments(i);
        If Doc.DM_DocumentKind = 'SCH' Then
        Begin
            SchDoc := SchServer.GetSchDocumentByPath(Doc.DM_FullPath);
            If SchDoc = Nil Then
            Begin
                Client.OpenDocument('SCH', Doc.DM_FullPath);
                SchDoc := SchServer.GetSchDocumentByPath(Doc.DM_FullPath);
            End;
            If SchDoc <> Nil Then
            Begin
                SchServer.ProcessControl.PreProcess(SchDoc, '');
                ProcessSheet(SchDoc);
                SchServer.ProcessControl.PostProcess(SchDoc, '');
                SrvDoc := Client.GetDocumentByPath(Doc.DM_FullPath);
                If SrvDoc <> Nil Then
                    SrvDoc.Modified := True;
            End;
        End;
    End;

    Missing := '';
    For i := 0 To Changes.Count - 1 Do
    Begin
        j := 0;
        While (j < Done.Count) And (Pos(Field(Changes[i], 0) + ' ->', Done[j]) <> 1) Do
            j := j + 1;
        If j >= Done.Count Then
            Missing := Missing + Field(Changes[i], 0) + ' ';
    End;

    If Missing <> '' Then
        ShowMessage('Cambiados:' + #13#10 + Done.Text + #13#10 + 'NO encontrados: ' + Missing)
    Else
        ShowMessage('Cambiados:' + #13#10 + Done.Text + #13#10 + 'Guarda los esquemáticos y haz Design > Update PCB Document.');

    Changes.Free;
    Done.Free;
End;
