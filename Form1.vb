Imports System.Deployment.Application
Imports System.Text
Imports System.IO.Ports
Imports System.Drawing.Printing
Imports System.Data.SqlClient

Public Class Form1

    Dim FAILED As String = "FAILED"
    Dim SUCCESS As String = "SUCCESS"
    Public LoginFlag As Boolean = False
    Dim timing_buffer As Integer = 1000

    'main entry point
    Private Sub Form1_Load(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles MyBase.Load
        StartFindingSerialPort()
        StartTmrProcess()
    End Sub

#Region "CashDuino"

    Public Buffer_Recv_SPORT1 As String = String.Empty

    Public WithEvents SerialPort1 As New IO.Ports.SerialPort
    Private Delegate Sub DisplayDelegate(ByVal displayChar As String)
    Dim COMByte As Byte()

    Dim CMD_CHECK_BOARD() As Byte = {&H2, &HB0, &H0, &H0, &H3, &HB3}
    Dim CMD_ENABLE_DISP() As Byte = {&H2, &HB1, &H0, &H0, &H3, &HB4}
    Dim CMD_DISABLE_DISP() As Byte = {&H2, &HB2, &H0, &H0, &H3, &HB5}
    Dim CMD_PULL_BUFF() As Byte = {&H2, &HB3, &H0, &H0, &H3, &HB6}

    Structure OperationCash
        Public A_COBRAR As Double       'MONTO FIJO GUARDA EL VALOR FIJO A COBRAR
        Public MONTO_FIJO As Double     'MONTO FIJO QUE SE COBRA
        Public MoneyIn As Double        'TOTAL DE DINERO QUE SE INGRESA
        Public operacion As String      'OPERACION, ID UNICO DEL CORTE EN PROGRESO
        Public status_oper As String    'ESTATUS DEL ID UNICO DE OPERACION, PUEDE SER "CLOSED" O "INPROGRESS"
        Public tipo As String           'TIPO, PUEDE SER "COMPLETE" O "PARTIAL" DE ACUERDO AL MONTO INGRESADO
        Public INPUT1000 As Integer     'BILL INSERTED COUNTER 1000
        Public INPUT500 As Integer      'BILL INSERTED COUNTER 500
        Public INPUT200 As Integer      'BILL INSERTED COUNTER 200
        Public INPUT100 As Integer      'BILL INSERTED COUNTER 100
        Public INPUT50 As Integer       'BILL INSERTED COUNTER 50
        Public INPUT20 As Integer       'BILL INSERTED COUNTER 20
    End Structure

    Dim DataCash As OperationCash

    Private Function BytetoHex(ByVal comByte As Byte()) As String
        Dim builder As New StringBuilder(comByte.Length * 3)
        For Each data As Byte In comByte
            builder.Append((Convert.ToString(data, 16).PadLeft(2, "0")))
        Next
        Return builder.ToString().ToUpper()
    End Function

    Private Sub Serialport1_DataReceived(ByVal sender As System.Object, _
                                         ByVal e As System.IO.Ports.SerialDataReceivedEventArgs) _
                                         Handles SerialPort1.DataReceived
        Dim RX_DATA As Integer
        RX_DATA = SerialPort1.BytesToRead

        If RX_DATA = 6 Then
            Dim RS232_Buff As Byte() = New Byte(RX_DATA - 1) {}
            SerialPort1.Read(RS232_Buff, 0, RX_DATA)
            Me.Invoke(New DisplayDelegate(AddressOf Processing_RX_Buffer), New Object() {BytetoHex(RS232_Buff)})
            SerialPort1.DiscardInBuffer()
            SerialPort1.DiscardOutBuffer()
            System.Threading.Thread.Sleep(timing_buffer)
        End If

        If ((RX_DATA > 0) And (RX_DATA <> 6)) Then
            SaveLogsERRORVM("CashDuino bad buffer size: " + Conversion.Str(RX_DATA))
            SerialPort1.DiscardInBuffer()
            SerialPort1.DiscardOutBuffer()
            ProcessFlag = False
            StringProcess = "Espere por favor"
            System.Threading.Thread.Sleep(timing_buffer)
            SendCMDtoVM(CMD_PULL_BUFF)
        End If


    End Sub

    Private Sub Processing_RX_Buffer(ByVal displayChar As String)

        Dim chars As Char() = displayChar.ToCharArray()
        Dim Buffer_uC_RX((chars.Length / 2) - 1) As String

        Dim OneChar As String
        Dim Cheksum As Integer = 0

        Dim FLagSTX As Boolean = False
        Dim PointCMD As Integer = 0
        Dim PointCHKSUM As Integer = 0

        Dim FLagETX As Boolean = False
        Dim PointETX As Integer = 0

        'AGRUPANDO DATOS 
        For i = 0 To (chars.Length / 2) - 1
            Try
                OneChar = (chars(i * 2) + chars((i * 2) + 1))
                Buffer_uC_RX(i) = OneChar
            Catch ex As Exception
            End Try
        Next

        SaveLogsVM("CashDuino answer: " + Buffer_uC_RX(0) + Buffer_uC_RX(1) + Buffer_uC_RX(2) + Buffer_uC_RX(3) + Buffer_uC_RX(4) + Buffer_uC_RX(5))

        'Se localiza si existe ETX = F2 y STX = F3
        If Buffer_uC_RX(0) = "F2" Then
            FLagSTX = True
            PointCMD = 1
        End If
        If Buffer_uC_RX(4) = "F3" Then
            FLagETX = True
            PointETX = 5
        End If

        If (FLagSTX = True) And (FLagETX = True) And (Buffer_uC_RX.Length = 6) Then

            For i = PointCMD To PointETX - 1
                Cheksum += Convert.ToInt32(Buffer_uC_RX(i), 16)
            Next
            Cheksum = Cheksum And 255
            PointCHKSUM = Buffer_uC_RX.Length - 1

            If Buffer_uC_RX(PointCMD) = "A0" And Convert.ToInt32(Buffer_uC_RX(PointCHKSUM), 16) = Cheksum Then
                ProcessFlag = False
                StringProcess = "Billetero apagado"
                TmrPortFound.Stop()
                EnableDisableControlls(True)
                System.Console.WriteLine("System Ready!")
            ElseIf Buffer_uC_RX(PointCMD) = "A1" And Convert.ToInt32(Buffer_uC_RX(PointCHKSUM), 16) = Cheksum Then
                ProcessFlag = True
                StringProcess = "Deposite efectivo"
                SaveLogsVM("Bill ready ...  ")
            ElseIf Buffer_uC_RX(PointCMD) = "A2" And Convert.ToInt32(Buffer_uC_RX(PointCHKSUM), 16) = Cheksum Then
                ProcessFlag = False
                StringProcess = "Billetero apagado"
                SaveLogsVM("Bill Disable ...  ")
            ElseIf Buffer_uC_RX(PointCMD) = "A3" And Convert.ToInt32(Buffer_uC_RX(PointCHKSUM), 16) = Cheksum Then
                ProcessFlag = True
                StringProcess = "Deposite efectivo"
                Select Case Convert.ToInt32(Buffer_uC_RX(2), 16)
                    Case &H80
                        SaveLogsVM("$20 Inserted ...  ")
                        DataCash.MoneyIn += 20
                        DataCash.INPUT20 += 1
                        Label4.Text = Amount_Format(DataCash.MoneyIn)
                        Label7.Text = DataCash.INPUT20
                        DataCash.A_COBRAR = DataCash.A_COBRAR - 20
                        Label6.Text = Amount_Format(DataCash.A_COBRAR)
                    Case &H81
                        SaveLogsVM("$50 Inserted ...  ")
                        DataCash.MoneyIn += 50
                        DataCash.INPUT50 += 1
                        Label4.Text = Amount_Format(DataCash.MoneyIn)
                        Label8.Text = DataCash.INPUT50
                        DataCash.A_COBRAR = DataCash.A_COBRAR - 50
                        Label6.Text = Amount_Format(DataCash.A_COBRAR)
                    Case &H82
                        SaveLogsVM("$100 Inserted ...  ")
                        DataCash.MoneyIn += 100
                        DataCash.INPUT100 += 1
                        Label4.Text = Amount_Format(DataCash.MoneyIn)
                        Label9.Text = DataCash.INPUT100
                        DataCash.A_COBRAR = DataCash.A_COBRAR - 100
                        Label6.Text = Amount_Format(DataCash.A_COBRAR)
                    Case &H83
                        SaveLogsVM("$200 Inserted ...  ")
                        DataCash.MoneyIn += 200
                        DataCash.INPUT200 += 1
                        Label4.Text = Amount_Format(DataCash.MoneyIn)
                        Label10.Text = DataCash.INPUT200
                        DataCash.A_COBRAR = DataCash.A_COBRAR - 200
                        Label6.Text = Amount_Format(DataCash.A_COBRAR)
                    Case &H84
                        SaveLogsVM("$500 Inserted ...  ")
                        DataCash.MoneyIn += 500
                        DataCash.INPUT500 += 1
                        Label4.Text = Amount_Format(DataCash.MoneyIn)
                        Label11.Text = DataCash.INPUT500
                        DataCash.A_COBRAR = DataCash.A_COBRAR - 500
                        Label6.Text = Amount_Format(DataCash.A_COBRAR)
                End Select

                If DataCash.MoneyIn >= DataCash.MONTO_FIJO Then
                    ProcessFlag = False
                    StringProcess = "Monto cubierto"
                    DataCash.tipo = "COMPLETE"
                    Save_Sale_SQL(DataCash)
                    PrintDocument1.Print()
                    clear_cash_objects()
                End If

            End If
        Else
            SaveLogsERRORVM("requesting last buffer from cashduino, bad payload formed: " + Buffer_uC_RX(0) + Buffer_uC_RX(1) + Buffer_uC_RX(2) + Buffer_uC_RX(3) + Buffer_uC_RX(4) + Buffer_uC_RX(5))
            SerialPort1.DiscardInBuffer()
            SerialPort1.DiscardOutBuffer()
            ProcessFlag = False
            StringProcess = "Espere por favor"
            System.Threading.Thread.Sleep(timing_buffer)
            SendCMDtoVM(CMD_PULL_BUFF)
        End If

    End Sub

    Private Sub SendCMDtoVM(ByVal Array() As Byte)
        Dim CMD_Ok_Send As Boolean = False

        Try
            SerialPort1.Write(Array, 0, Array.Length)
            CMD_Ok_Send = True
            SaveLogsVM("Send to CashDuino: " + Hex(Array(0)) + Hex(Array(1)) + Hex(Array(2)) + Hex(Array(3)) + Hex(Array(4)) + Hex(Array(5)))
        Catch ex As Exception
            'something goes wrong
            SaveLogsERRORVM("SendCMDtoVM: " & NamePortArray(PointerNamePort) & ex.ToString)
        End Try

        If CMD_Ok_Send = False Then
            StartFindingSerialPort()
        End If

    End Sub

    Public Function Amount_Format(ByVal amount As Double) As String
        Return FormatCurrency(amount.ToString, , , Microsoft.VisualBasic.TriState.True, Microsoft.VisualBasic.TriState.True)
    End Function
#End Region

#Region "FIND_SERIAL_PORT"
    Dim TmrPortFound As New System.Windows.Forms.Timer 'Tag = 1 TIMER PARA BUSCAR EL PUERTO SERIAL Y ANCLARLO.
    Dim NamePortArray() As String = {"COM6", "COM6", "COM6", "COM6", "COM6", "COM6", "COM6", "COM6", "COM6", "COM6", "COM6"}
    Dim PointerNamePort As Integer = 0
    Dim ShotsPort As Integer = 0

    Dim TmrProcessData As New System.Windows.Forms.Timer
    Dim Count1 As Integer = 0
    Dim ProcessFlag As Boolean = True
    Dim StringProcess As String = "Espere por favor"

    Private Sub SetUpTimerHome_PORT()
        TmrPortFound.Interval = 100
        TmrPortFound.Enabled = True
        TmrPortFound.Stop()
        TmrPortFound.Tag = 1
        AddHandler TmrPortFound.Tick, AddressOf TimerTick_SearchPort
    End Sub

    Private Sub TimerTick_SearchPort(ByVal sender As Object, ByVal e As EventArgs)
        Try
            If PointerNamePort < NamePortArray.Length Then
                If SerialPort1.IsOpen Then
                    SerialPort1.Dispose()
                    SerialPort1.Close()
                End If
                SerialPort1.PortName = NamePortArray(PointerNamePort)
                SerialPort1.DataBits = 8
                SerialPort1.BaudRate = 115200
                SerialPort1.Parity = Parity.None
                SerialPort1.StopBits = StopBits.One
                SaveLogsVM("Try to open port: " & NamePortArray(PointerNamePort))
                ShotsPort += 1
                If ShotsPort = 3 Then
                    ShotsPort = 0
                    PointerNamePort += 1
                End If
                SerialPort1.Open()
                SendCMDtoVM(CMD_CHECK_BOARD)
            Else
                PointerNamePort = 0
            End If
        Catch ex As Exception
            If (PointerNamePort < NamePortArray.Length) Then
                SaveLogsERRORVM(NamePortArray(PointerNamePort) & ex.ToString)
            Else
                SaveLogsERRORVM(NamePortArray(PointerNamePort - 1) & ex.ToString)
            End If

        End Try
    End Sub

    Public Sub StartFindingSerialPort()
        SetUpTimerHome_PORT()
        PointerNamePort = 0
        TmrPortFound.Start()
    End Sub

    Private Sub SetUpTmrProcess()
        TmrProcessData.Interval = 100
        TmrProcessData.Enabled = True
        TmrProcessData.Stop()
        TmrProcessData.Tag = 2
        AddHandler TmrProcessData.Tick, AddressOf TimerTick_Process
    End Sub

    Private Sub TimerTick_Process(ByVal sender As Object, ByVal e As EventArgs)

        RadioButton1.Text = StringProcess

        If ProcessFlag = True Then
            Count1 += 1
            If Count1 = 1 Then
                RadioButton1.Checked = True
            ElseIf Count1 = 2 Then
                RadioButton1.Checked = False
                Count1 = 0
            End If
        Else
            RadioButton1.Checked = False
        End If
    End Sub

    Public Sub StartTmrProcess()
        SetUpTmrProcess()
        TmrProcessData.Start()
    End Sub

#End Region

#Region "Tools"
    Public Sub EnableDisableControlls(ByVal mode As Boolean)
        Dim ctrl As Control
        For Each ctrl In Me.Controls
            ctrl.Enabled = mode
        Next
    End Sub
#End Region


#Region "Logs_Data"

    Dim FlagToClear1 As Integer = 0
    Dim MessageTxt_Vending As String = ""

    Public Sub SaveLogsVM(ByVal MessageTxt As String)

        Dim MyYear As String
        Dim MyMonth As String
        Dim Myday As String

        MyYear = Today.Year
        MyMonth = Today.Month
        Myday = Today.Day

        Dim PathData As String = "C:\LogsSelPay\"
        Dim FILE_NAME As String = PathData & "LOG-" & Myday & "-" & MyMonth & "-" & MyYear & ".txt"

        Try
            If (Not System.IO.Directory.Exists(PathData)) Then
                System.IO.Directory.CreateDirectory(PathData)
            End If
            If System.IO.File.Exists(FILE_NAME) = False Then
                System.IO.File.Create(FILE_NAME).Dispose()
            End If
            Dim objWriter As New System.IO.StreamWriter(FILE_NAME, True)
            objWriter.WriteLine(MessageTxt & "   " & "[" & "20" & DateTime.Now.ToString("yy/MM/dd") & "   -   " & Now.ToString("HH:mm:ss.fff tt") & "]")
            objWriter.Close()
        Catch ex As Exception

        End Try
    End Sub

    Public Sub SaveLogsERRORVM(ByVal MessageTxt As String)

        Dim MyYear As String
        Dim MyMonth As String
        Dim Myday As String

        MyYear = Today.Year
        MyMonth = Today.Month
        Myday = Today.Day

        Dim PathData As String = "C:\LogsSelPay\"
        Dim FILE_NAME As String = PathData & "ERR-" & Myday & "-" & MyMonth & "-" & MyYear & ".txt"

        Try
            If (Not System.IO.Directory.Exists(PathData)) Then
                System.IO.Directory.CreateDirectory(PathData)
            End If
            If System.IO.File.Exists(FILE_NAME) = False Then
                System.IO.File.Create(FILE_NAME).Dispose()
            End If
            Dim objWriter As New System.IO.StreamWriter(FILE_NAME, True)
            objWriter.WriteLine(MessageTxt & "   " & "[" & "20" & DateTime.Now.ToString("yy/MM/dd") & "   -   " & Now.ToString("HH:mm:ss.fff tt") & "]")
            objWriter.Close()
        Catch ex As Exception

        End Try
    End Sub
#End Region


End Class
