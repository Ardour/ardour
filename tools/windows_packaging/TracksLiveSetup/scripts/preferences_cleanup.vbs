' deletes Tracks Live Preferences 
' except recent and config 
Sub DeleteFile(filespec)
   Dim fso
   Set fso = CreateObject("Scripting.FileSystemObject")
   If (fso.FileExists(filespec)) Then
	fso.DeleteFile(filespec)
   End If
End Sub

Dim objNetwork
Set objNetwork = CreateObject("WScript.Network")
strUserName = objNetwork.UserName
path_to_preferences="c:\Users\"+strUserName+"\AppData\Local\Tracks Live\.config\"

Call DeleteFile (path_to_preferences+"instant.xml")
Call DeleteFile (path_to_preferences+"sfdb")
Call DeleteFile (path_to_preferences+"ui_config")

path_to_export = path_to_preferences+"export"
Dim fso
Set fso = CreateObject("Scripting.FileSystemObject")
If (fso.FolderExists(path_to_export)) Then
	fso.DeleteFolder(path_to_export)
End If



