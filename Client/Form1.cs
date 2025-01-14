﻿using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using static System.Windows.Forms.VisualStyles.VisualStyleElement.TextBox;
using System.Xml.Linq;
using System.Drawing.Drawing2D;
using System.Net.Sockets;
using System.IO;
using System.Runtime.InteropServices.ComTypes;
using System.Security.Cryptography;
//using System.Timers;

namespace MultiSaper
{
    public partial class Form1 : Form
    {
        //0 - 8 BLANK COVERED ADJACENT TO BOMBS
        //10 - 18 BLANK UNCOVERED ADJACENT TO BOMBS
        //20 - 28 BLANK MISFLAG
        const int BOMB_COVERED = 31;
        const int BOMB_UNCOVERED_OFF = 32;
        const int BOMB_UNCOVERED_OFF_ME = 82;
        const int BOMB_UNCOVERED_FLAG = 33;
        const int BOMB_UNCOVERED_FLAG_ME = 83;
        //60 - 68 BLANK UNCOVERED_ME ADJACENT TO BOMBS
        //70 - 78 BLANK MISFLAG_ME

        const int GAME_STATE_IDLE = 0;
        const int GAME_STATE_WAITING = 1;
        const int GAME_STATE_GAME = 2;
        const int GAME_STATE_OVER = 3;

        const int SEND_LOGIN = 49;
        const int SEND_LEFTCLICK = 50;
        const int SEND_RIGHTCLICK = 51;
        const int SEND_GAME_STATE = 80;
        const int SEND_BOARD_DATA = 81;
        const int SEND_SCORE_DATA = 82;
        const int SEND_PLAYER_DATA = 83;
        const int SEND_WRONG_NAME = 90;

        int _sizeBoard;
        int _sizeX;
        int _sizeY;
        int[] _board = null;
        int _board_edge;


        int _offsetX = 0; //w polach
        int _offsetY = 0; //w polach
        int _rozmiar_pola = 24;
        int _pasek_naglowka = 10;
        
        int _player_id = 0;
        int _gameTimer = 0;
        int _gameState = GAME_STATE_IDLE;

        int _cmdPending = 0;
        int _bytesNeeded = 0;
        List<byte> _partMsg = new List<byte>();
        List<byte> _data = new List<byte>();

        bool _writing = false;
        bool _showMsgBox = false;

        private Point _pt1Scr = Point.Empty;
        private PointF _ptOffsetOld = new PointF(0, 0);
        private PointF _ptOffset = new PointF(0, 0);
        private PointF _ptOffsetMax = new PointF(0, 0);

        ArrayList _playersList = new ArrayList();
        ArrayList myClicksPast = new ArrayList();
        ArrayList myClicksCurr = new ArrayList();
        private TcpClient _tcpClient = null;
        NetworkStream _stream;
        IAsyncResult _result;
        AsyncCallback _callBackEndRead;
        AsyncCallback _callBackEndWrite;
        byte[] _rxBuf = new byte[8192];

        private static Timer timer;

        public Form1()
        {
            InitializeComponent();
        }
        private void Form1_Load(object sender, EventArgs e)
        {

            Random r = new Random();
            int liczba_losowa = r.Next(999999);
            textBoxLognname.Text = "Player_" + liczba_losowa.ToString();
            textBoxIPAddress.Text = "192.168.1.80";
            textBoxPort.Text = "3000";
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            DisconnectServer();
        }

        private void UpdateTopScores()
        {
            _playersList.Sort();
            listView1.Invoke((MethodInvoker)delegate
            {
                listView1.Items.Clear();
                foreach (Player pl in _playersList)
                {
                    ListViewItem listViewItem = new ListViewItem(new string[] { pl.name, pl.score.ToString() }, -1);
                    listView1.Items.Add(listViewItem);
                }
            });
        }

        private Player GetPlayer(int id)
        {
            foreach (Player pl in _playersList)
            {
                if (pl.id == id) return pl;
            }
            return null;
        }

        private int getPole(PointF pt)
        {
            int pole = 0;
            int column = (int)Math.Floor((double)((pt.X - _pasek_naglowka) / _rozmiar_pola));
            int row = (int)Math.Floor((double)((pt.Y - _pasek_naglowka) / _rozmiar_pola));

            pole = (row + _offsetY) * _sizeX + column + _offsetX;

            return pole;
        }

        private void pictureBoxBoard_Paint(object sender, PaintEventArgs e)
        {

            if(_board == null) return;
            


            int box_width = pictureBoxBoard.Width - _pasek_naglowka;
            int box_height = pictureBoxBoard.Height - _pasek_naglowka;
            int columns = (int)Math.Floor((double)(box_width / _rozmiar_pola));
            int rows = (int)Math.Floor((double)(box_height / _rozmiar_pola));

            _ptOffsetMax.X = (_sizeX - columns + 1) * _rozmiar_pola;
            _ptOffsetMax.Y = (_sizeY - rows + 1) * _rozmiar_pola;
            hScrollBar1.Maximum = (int)_ptOffsetMax.X;
            vScrollBar1.Maximum = (int)_ptOffsetMax.Y;
            hScrollBar1.Value = (int)Math.Min(_ptOffset.X, _ptOffsetMax.X);
            vScrollBar1.Value = (int)Math.Min(_ptOffset.Y, _ptOffsetMax.Y);

            _offsetX = (int)Math.Floor((double)(_ptOffset.X / _rozmiar_pola));
            if (_offsetX < 0) _offsetX = 0;
            if (_offsetX > _sizeX - columns) _offsetX = _sizeX - columns;
            _offsetY = (int)Math.Floor((double)(_ptOffset.Y / _rozmiar_pola));
            if (_offsetY < 0) _offsetY = 0;
            if (_offsetY > _sizeY - rows) _offsetY = _sizeY - rows;


            Graphics g = e.Graphics;
            Pen penBlack = new Pen(Color.Black);
            Pen penGray = new Pen(Color.Gray);
            Pen penRed = new Pen(Color.Red);
            Pen penWhite = new Pen(Color.White);
            Pen penBlue = new Pen(Color.Blue, 3);
            SolidBrush brushBlack = new SolidBrush(Color.Black);
            SolidBrush brushRed = new SolidBrush(Color.Red);
            SolidBrush brushGreen = new SolidBrush(Color.Green);
            Font fontArial8 = new Font("Arial", 8f, FontStyle.Regular, GraphicsUnit.Pixel);
            Font fontArial12 = new Font("Arial", 12f, FontStyle.Bold, GraphicsUnit.Pixel);

            for (int i = 0; i <= columns; i++)
            {
                String colname = (_offsetX + i).ToString();
                SizeF sz = g.MeasureString(colname, fontArial8);
                g.DrawString(colname, fontArial8, brushBlack, i * _rozmiar_pola + _rozmiar_pola / 2 + _pasek_naglowka - (sz.Width / 2), 2);
                g.DrawLine(penGray, i * _rozmiar_pola + _pasek_naglowka, 0, i * _rozmiar_pola + _pasek_naglowka, box_height);
            }
            for (int i = 0; i <= rows; i++)
            {
                GraphicsState transState = g.Save();
                g.TranslateTransform(0, i * _rozmiar_pola + _pasek_naglowka);
                g.RotateTransform(-90);
                String rowname = (_offsetY + i).ToString();
                SizeF sz = g.MeasureString(rowname, fontArial8);
                g.DrawString(rowname, fontArial8, brushBlack, -_rozmiar_pola / 2 - (sz.Width / 2), 0);
                g.ResetTransform();
                g.Restore(transState);
                g.DrawLine(penGray, 0, i * _rozmiar_pola + _pasek_naglowka, box_width, i * _rozmiar_pola + _pasek_naglowka);
            }

            for (int i = 0; i < rows; i++)
                for (int j = 0; j < columns; j++)
                {

                    int state = _board[(i + _offsetY) * _sizeX + j + _offsetX];
                    float px = j * _rozmiar_pola + _pasek_naglowka;
                    float py = i * _rozmiar_pola + _pasek_naglowka;

                    if (state < 9)
                    {
                        g.DrawRectangle(penBlack, px + 2, py + 2, 20, 20);
                    }
                    else if (state == 10)
                    {
                        //draw nothing
                    }
                    else if ((state > 10) && (state < 19))
                    {
                        int val = state - 10;
                        SizeF sz = g.MeasureString(val.ToString(), fontArial12);
                        g.DrawString(val.ToString(), fontArial12, brushBlack,
                            px + 12 - (sz.Width / 2), py + 12 - (sz.Height / 2));
                    }
                    else if (state == 20) //MISFLAG uncovered 0
                    {
                        PointF[] triang = { new PointF(px + 6, py + 4), new PointF(px + 6, py + 20), new PointF(px + 20, py + 12) };
                        g.DrawPolygon(penBlack, triang);
                        g.DrawRectangle(penBlue, px + 2, py + 2, 20, 20);

                    }
                    else if ((state > 20) && (state < 29)) //+50 = _ME
                    {
                        PointF[] triang = { new PointF(px + 6, py + 4), new PointF(px + 6, py + 20), new PointF(px + 20, py + 12) };
                        g.DrawPolygon(penBlack, triang);
                        int val = state - 20;
                        SizeF sz = g.MeasureString(val.ToString(), fontArial12);
                        g.DrawString(val.ToString(), fontArial12, brushBlack,
                            px + 12 - (sz.Width / 2), py + 12 - (sz.Height / 2));

                    }
                    else if (state == 60) //uncovered 0 _ME
                    {
                        g.DrawRectangle(penBlue, px + 2, py + 2, 20, 20);

                    }
                    else if ((state > 60) && (state < 69)) //+50 = _ME
                    {
                        int val = state - 60;
                        SizeF sz = g.MeasureString(val.ToString(), fontArial12);
                        g.DrawString(val.ToString(), fontArial12, brushBlack,
                            px + 12 - (sz.Width / 2), py + 12 - (sz.Height / 2));
                        g.DrawRectangle(penBlue, px + 2, py + 2, 20, 20);


                    }
                    else if (state == 70) //MISFLAG_ME uncovered 0
                    {
                        PointF[] triang = { new PointF(px + 6, py + 4), new PointF(px + 6, py + 20), new PointF(px + 20, py + 12) };
                        g.DrawPolygon(penBlack, triang);
                        g.DrawRectangle(penBlue, px + 2, py + 2, 20, 20);

                    }
                    else if ((state > 70) && (state < 79)) //+50 = _ME
                    {
                        PointF[] triang = { new PointF(px + 6, py + 4), new PointF(px + 6, py + 20), new PointF(px + 20, py + 12) };
                        g.DrawPolygon(penBlack, triang);
                        g.DrawRectangle(penBlue, px + 2, py + 2, 20, 20);
                        int val = state - 70;
                        SizeF sz = g.MeasureString(val.ToString(), fontArial12);
                        g.DrawString(val.ToString(), fontArial12, brushBlack,
                            px + 12 - (sz.Width / 2), py + 12 - (sz.Height / 2));
                    }
                    else if (state == BOMB_COVERED)
                    {
                        g.DrawRectangle(penBlack, px + 2, py + 2, 20, 20);
                        //debug only
                        //g.DrawRectangle(penBlack, px + 8, py + 8, 8, 8);
                    }
                    else if (state == BOMB_UNCOVERED_OFF)
                    {
                        g.FillEllipse(brushRed, j * _rozmiar_pola + _pasek_naglowka + 6, i * _rozmiar_pola + _pasek_naglowka + 6, 12, 12);
                    }
                    else if (state == BOMB_UNCOVERED_OFF_ME)
                    {
                        g.FillEllipse(brushRed, j * _rozmiar_pola + _pasek_naglowka + 6, i * _rozmiar_pola + _pasek_naglowka + 6, 12, 12);
                        g.DrawRectangle(penBlue, px + 2, py + 2, 20, 20);

                    }
                    else if (state == BOMB_UNCOVERED_FLAG)
                    {
                        PointF[] triang = { new PointF(px + 6, py + 4), new PointF(px + 6, py + 20), new PointF(px + 20, py + 12) };
                        g.FillPolygon(brushGreen, triang);
                    }
                    else if (state == BOMB_UNCOVERED_FLAG_ME)
                    {
                        PointF[] triang = { new PointF(px + 6, py + 4), new PointF(px + 6, py + 20), new PointF(px + 20, py + 12) };
                        g.FillPolygon(brushGreen, triang);
                        g.DrawRectangle(penBlue, px + 2, py + 2, 20, 20);

                    }

                }

        }

        private void pictureBoxBoard_MouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                int polenr = getPole(e.Location);
                if (polenr < _sizeBoard)
                    SendClick(polenr, SEND_RIGHTCLICK);
            }
        }

        private void pictureBoxBoard_MouseDown(object sender, MouseEventArgs e)
        {
            _pt1Scr = Point.Empty;

            if (e.Button == MouseButtons.Left)
            {
                _pt1Scr = e.Location;
                _ptOffsetOld = _ptOffset;
            }
        }

        private void pictureBoxBoard_MouseMove(object sender, MouseEventArgs e)
        {

            if (_pt1Scr.IsEmpty == false)
            {
                Point ptMouse = e.Location;
                _ptOffset = _ptOffsetOld;
                if ((Math.Abs(ptMouse.X - _pt1Scr.X) > _rozmiar_pola * 2 || (Math.Abs(ptMouse.Y - _pt1Scr.Y) > _rozmiar_pola * 2)))
                {
                    _ptOffset.X += _pt1Scr.X - e.Location.X;
                    if (_ptOffset.X < 0) _ptOffset.X = 0;
                    if (_ptOffset.X > _ptOffsetMax.X) _ptOffset.X = _ptOffsetMax.X;
                    _ptOffset.Y += _pt1Scr.Y - e.Location.Y;
                    if (_ptOffset.Y < 0) _ptOffset.Y = 0;
                    if (_ptOffset.Y > _ptOffsetMax.Y) _ptOffset.Y = _ptOffsetMax.Y;

                    pictureBoxBoard.Invalidate();
                }
            }
        }

        private void pictureBoxBoard_MouseUp(object sender, MouseEventArgs e)
        {

            {
                if (e.Button == MouseButtons.Left)
                {
                    Point ptMouse = e.Location;
                    _ptOffset = _ptOffsetOld;
                    if ((Math.Abs(ptMouse.X - _pt1Scr.X) > _rozmiar_pola*2 || (Math.Abs(ptMouse.Y - _pt1Scr.Y) > _rozmiar_pola*2)))
                    {
                        _ptOffset.X += _pt1Scr.X - e.Location.X;
                        if (_ptOffset.X < 0) _ptOffset.X = 0;
                        if (_ptOffset.X > _ptOffsetMax.X) _ptOffset.X = _ptOffsetMax.X;
                        _ptOffset.Y += _pt1Scr.Y - e.Location.Y;
                        if (_ptOffset.Y < 0) _ptOffset.Y = 0;
                        if (_ptOffset.Y > _ptOffsetMax.Y) _ptOffset.Y = _ptOffsetMax.Y;
                    }
                    else
                    {
                        int polenr = getPole(ptMouse);
                        if (polenr < _sizeBoard)
                            SendClick(polenr, SEND_LEFTCLICK);
                    }
                    pictureBoxBoard.Invalidate();
                    _ptOffsetOld = _pt1Scr = Point.Empty;
                }
            }

        }

        private void setupBoard()
        {
            _sizeX = _board_edge;
            _sizeY = _board_edge;
            _sizeBoard = _board_edge * _board_edge;

            _board = new int[_sizeBoard];
            pictureBoxBoard.Invoke((MethodInvoker)delegate
            {
                pictureBoxBoard.MaximumSize = new Size((_board_edge+1) * _rozmiar_pola -6, (_board_edge+1) * _rozmiar_pola -6);
            });
        }



        private void ConnectServer()
        {

            if ((_tcpClient != null) && (_tcpClient.Connected)) return;

            string adresIP = textBoxIPAddress.Text;
            int port;
            try
            {
                port = int.Parse(textBoxPort.Text);

            }
            catch (Exception) {
                MessageBox.Show("Error! Wrong port number");
                return;
            }

            try
            {
                
                textBoxTCPIPClient.SelectedText = "Connecting server ...\r\n";
                _tcpClient = new TcpClient(adresIP, port);
            }
            catch (SocketException)
            {
                textBoxTCPIPClient.SelectedText = "Network error! Connection couldn't be established.\r\n";
                return;
            }
            catch (IOException)
            {
                textBoxTCPIPClient.SelectedText = "IO error! Connection couldn't be established.\r\n";
                return;
            }

            textBoxTCPIPClient.SelectedText = "Server connected.\r\n";

            _stream = _tcpClient.GetStream();
            _callBackEndRead = new AsyncCallback(GetData);
            _callBackEndWrite = new AsyncCallback(FinishWrite);
            try
            {
                _result = _stream.BeginRead(_rxBuf, 0, 4, _callBackEndRead, null);
            }
            catch(IOException)
            {
                textBoxTCPIPClient.SelectedText = "IO error! Read from stream couldn't be completed.\r\n";
                return;
            }
        }

        private void DisconnectServer()
        {
            if ((_tcpClient != null) && (_tcpClient.Connected))
            {
                if (_stream != null) _stream.Close();
                _tcpClient.Close();
                _tcpClient = null;
            }
        }

        void DebugWriteSafe(string msg)
        {
            textBoxTCPIPClient.Invoke((MethodInvoker)delegate
            {
                textBoxTCPIPClient.SelectedText = msg + "\r\n";
            });
        }
        private void FinishWrite(IAsyncResult result)
        {
            try
            {
                _stream.EndWrite(result);
                _writing = false;
            }
            catch (Exception ex)
            {

                DebugWriteSafe("Finish write exception " + ex.Message);
                return;
            }
        }
        
        private void GetData(IAsyncResult result)
        {
            int bytesReceived;
            try
            {
                bytesReceived = _stream.EndRead(result);

            }
            catch (Exception)
            {
                buttonLogin.Invoke((MethodInvoker)delegate
                {
                    buttonLogin.Enabled = true;

                });


                labelInfo.Invoke((MethodInvoker)delegate
                {
                    labelInfo.Text = "Press <Play> to join the game";
                });

                DebugWriteSafe("Connection lost");
                DisconnectServer();
                _gameState = GAME_STATE_IDLE;

                return;
            }

            
            if (bytesReceived > 0)
            {

                _partMsg.AddRange(_rxBuf.Take(bytesReceived));


                if (bytesReceived < _bytesNeeded)
                {
                    _bytesNeeded = _bytesNeeded - bytesReceived;

                }
                else
                {

                    if (_cmdPending == 0)
                    {

                        if (_partMsg[0] == SEND_WRONG_NAME)
                        {
                            labelInfo.Invoke((MethodInvoker)delegate
                            {
                                labelInfo.Text = "Press <Play> to join the game";
                            });
                            buttonLogin.Invoke((MethodInvoker)delegate
                            {
                                buttonLogin.Enabled = true;
                            });

                            DisconnectServer();
                            _gameState = GAME_STATE_IDLE;
                            DebugWriteSafe("Wrong name entered");
                            MessageBox.Show("Ten nick jest już zajęty");

                        }
                        else
                        {
                            _cmdPending = _partMsg[0];
                            _bytesNeeded = (int)_partMsg[1] | (int)_partMsg[2] << 8 | (int)_partMsg[3] << 16;
                            DebugWriteSafe("Bytes needed:" + _bytesNeeded.ToString() + " cmd: " + _cmdPending.ToString());
                        }
                    }
                    else
                    {

                        int cmd = _cmdPending;

                        DebugWriteSafe("Bytes recieved:" + bytesReceived.ToString() + " cmd: " + _cmdPending.ToString());

                        _cmdPending = 0;
                        _bytesNeeded = 4;
                        if (_player_id == 0) //player not logged in
                        {
                            if (cmd == SEND_LOGIN) //login response
                            {
                                _player_id = (int)_partMsg[0] | (int)_partMsg[1] << 8 | (int)_partMsg[2] << 16;
                                _board_edge = (int)_partMsg[3] | (int)_partMsg[4] << 8;
                                setupBoard();

                                DebugWriteSafe("Player ID received: " + _player_id.ToString());
                                Player pl = new Player(_player_id, 0, textBoxLognname.Text);
                                _playersList.Add(pl);
                                UpdateTopScores();
                            }
                        }
                        else //player logged in
                        {
                            if (cmd == SEND_GAME_STATE) //game state
                            {
                                int prevState = _gameState;
                                int cnt = bytesReceived / 3;

                                int rs = 3; //record size = 1b state, 2b time
                                for (int i = 0; i < cnt; i++)
                                {
                                    _gameState = (int)_partMsg[i * rs];
                                    _gameTimer = (int)_partMsg[i * rs + 1] | (int)_partMsg[i * rs + 2] << 8;
                                }
                                String info = "Press <Play> to join the game";
                                switch (_gameState)
                                {
                                    case GAME_STATE_IDLE:
                                        info = "Press <Play> to join the game";
                                        if (prevState > GAME_STATE_IDLE)
                                        {
                                            
                                            labelInfo.Invoke((MethodInvoker)delegate
                                            {
                                                buttonLogin.Enabled = true;
                                            });
                                            DisconnectServer();
                                        }
                                        break;
                                    case GAME_STATE_WAITING:
                                        info = "Time to start :" + _gameTimer.ToString();
                                        if (prevState < GAME_STATE_WAITING) pictureBoxBoard.Invalidate();//display board
                                        break;
                                    case GAME_STATE_GAME:
                                        info = "Remaining time : " + _gameTimer.ToString();
                                        if (prevState < GAME_STATE_GAME)
                                        {
                                            timer1.Enabled = true;
                                            pictureBoxBoard.Invalidate();//player joined after start
                                        }
                                        break;
                                    case GAME_STATE_OVER:
                                        info = "Game over!";
                                        if (prevState < GAME_STATE_OVER) _showMsgBox=true;

                                        break;
                                }

                                labelInfo.Invoke((MethodInvoker)delegate
                                {
                                    labelInfo.Text = info;
                                    if (_showMsgBox)
                                    {
                                        MessageBox.Show(this, "Game over!");
                                        _showMsgBox = false;
                                    }
                                });
                            }
                            else if (cmd == SEND_BOARD_DATA)//board update
                            {
                                int cnt = bytesReceived / 6;
                                DebugWriteSafe("Board update received: " + cnt.ToString());
                                int rs = 6; //record size = 3b num, 3b data
                                for (int i = 0; i < cnt; i++)
                                {
                                    int num = (int)_partMsg[i * rs + 0] | (int)_partMsg[i * rs + 1] << 8 | (int)_partMsg[i * rs + 2] << 16;
                                    int data = (int)_partMsg[i * rs + 3] | (int)_partMsg[i * rs + 4] << 8 | (int)_partMsg[i * rs + 5] << 16;
                                    int plid = (int)(data / 100);
                                    int val = (int)(data % 100);
                                    if (plid == _player_id)
                                    {
                                        _board[num] = val + 50;
                                        myClicksCurr.Add(num);
                                    }
                                    else
                                        _board[num] = val;
                                }

                                pictureBoxBoard.Invalidate();
                            }
                            else if (cmd == SEND_PLAYER_DATA) //add players 
                            {
                                int cnt = bytesReceived / 38;
                                DebugWriteSafe("New players data received: " + cnt.ToString());
                                int rs = 38; //record size = 3b id, 3b score, 32b name
                                for (int i = 0; i < cnt; i++)
                                {
                                    int id = (int)_partMsg[i * rs] | (int)_partMsg[i * rs + 1] << 8 | (int)_partMsg[i * rs + 2] << 16;
                                    int score = (int)_partMsg[i * rs + 3] | (int)_partMsg[i * rs + 4] << 8 | (int)_partMsg[i * rs + 5] << 16;
                                    string name = Encoding.ASCII.GetString(_partMsg.ToArray(), i * rs + 6, 32);
                                    if (id != _player_id)
                                    {
                                        Player pl = new Player(id, score, name);
                                        _playersList.Add(pl);
                                    }
                                }
                                UpdateTopScores();
                            }
                            else if (cmd == SEND_SCORE_DATA) //players' scores update
                            {
                                int cnt = bytesReceived / 6;
                                DebugWriteSafe("Players' scores update received: " + cnt.ToString());
                                for (int i = 0; i < cnt; i++)
                                {
                                    int id = (int)_partMsg[i * 6 + 0] | (int)_partMsg[i * 6 + 1] << 8 | (int)_partMsg[i * 6 + 2] << 16;
                                    int score = (int)_partMsg[i * 6 + 3] | (int)_partMsg[i * 6 + 4] << 8 | (int)_partMsg[i * 6 + 5] << 16;
                                    if (id == _player_id)
                                    {
                                        labelScore.Invoke((MethodInvoker)delegate
                                        {
                                            labelScore.Text = "Score: " + score.ToString();
                                        });
                                    }
                                    Player pl = GetPlayer(id);
                                    pl.score = score;
                                }
                                UpdateTopScores();
                            }
                            else
                            {
                                DebugWriteSafe("Unknown data received: " + cmd.ToString());

                            }
                        }
                    }
                }
                _partMsg.Clear();

            } else if (bytesReceived == 0) 
            {
                buttonLogin.Invoke((MethodInvoker)delegate
                {
                    buttonLogin.Enabled = true;

                });


                labelInfo.Invoke((MethodInvoker)delegate
                {
                    labelInfo.Text = "Press <Play> to join the game";
                });

                DebugWriteSafe("Connection lost");

                DisconnectServer();
                _gameState = GAME_STATE_IDLE;
            }
            if ((_tcpClient != null) && (_tcpClient.Connected))
            {
                try
                {
                    _result = _stream.BeginRead(_rxBuf, 0, _bytesNeeded, _callBackEndRead, null);
                }
                catch (ObjectDisposedException)
                {
                    DebugWriteSafe("Network Stream error! Connection couldn't be established.");
                }
                catch (IOException)
                {
                    DebugWriteSafe("IO error! Connection couldn't be established.");
                }
            }
        }

        private void StartToWrite(object sender, EventArgs e)
        {
            if (sender is Timer) DebugWriteSafe("Problems with sending data");

            if (!_writing)
            {

                if(timer != null && timer.Enabled) timer.Stop();

                _writing = true;
                byte[] data = _data.ToArray();

                try
                {
                    _stream.BeginWrite(data, 0, data.Length, _callBackEndWrite, null);
                }
                catch (ArgumentNullException ex)
                {
                    Console.WriteLine("ArgumentNullException: {0}", ex);
                }
                catch (IOException ex)
                {
                    MessageBox.Show("Brak połączenia z serwerem");

                    Console.WriteLine("IOException: {0}", ex);

                    buttonLogin.Enabled = true;

                    DisconnectServer();
                    _gameState = GAME_STATE_IDLE;
                }
                catch (SocketException ex)
                {
                    Console.WriteLine("SocketException: {0}", ex);
                }
                _data.Clear();
            }
            else
            {
                if (timer == null)
                {
                    timer = new Timer();
                    timer.Interval = 1000;
                    timer.Tick += StartToWrite;
                } else if(timer.Enabled)
                {
                    timer.Stop();
                }
                timer.Start();
            }

                    
        }

        private void SendClick(int num, int mousebutton)
        {
            if ((_player_id > 0) && (_gameState == GAME_STATE_GAME))
            {
                byte[] data = { (byte)(mousebutton), (byte)(num), (byte)(num >> 8), (byte)(num >> 16) };
                _data.AddRange(new List<byte> { (byte)(mousebutton), (byte)(num), (byte)(num >> 8), (byte)(num >> 16) });

                StartToWrite(this, EventArgs.Empty);
                
            }
        }
        private void buttonLogin_Click(object sender, EventArgs e)
        {
            ConnectServer();
            if (_tcpClient == null)
            {
                MessageBox.Show("Can't connect to the server");
            }
            else if (_tcpClient.Connected)
            {

                    String login = "1" + textBoxLognname.Text;//1 = ascii 49 is command code for log in
                    Byte[] data = Encoding.ASCII.GetBytes(login);

                    if (data.Length < 33)
                    {
                        Array.Resize(ref data, 33);
                    }
                    _data.AddRange(data);
                    StartToWrite(this, EventArgs.Empty);

                buttonLogin.Enabled = false;

                _player_id = 0;
                _gameTimer = 0;
                _offsetX = 0; //w polach
                _offsetY = 0; //w polach
                _ptOffset.X = 0;
                _ptOffset.Y = 0;
                _playersList.Clear();//clear topscores
                listView1.Items.Clear();
                labelScore.Text = "Score: 0";
                for (int i = 0; i < _sizeBoard; i++) _board[i] = 0;//clear the board
            }
        }

        private void hScrollBar1_Scroll(object sender, ScrollEventArgs e)
        {
            _ptOffset.X = e.NewValue;
            pictureBoxBoard.Invalidate();
        }

        private void vScrollBar1_Scroll(object sender, ScrollEventArgs e)
        {
            _ptOffset.Y = e.NewValue;
            pictureBoxBoard.Invalidate();
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            foreach (int i in myClicksPast)
            {
                _board[i] -= 50;
            }
            myClicksPast = myClicksCurr;
            myClicksCurr = new ArrayList();
        }

        private void Form1_SizeChanged(object sender, EventArgs e)
        {
            pictureBoxBoard.Invalidate();
        }

    }
}
