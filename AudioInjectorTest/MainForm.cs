using System;
using System.Windows.Forms;
using NAudio.CoreAudioApi;
using System.Runtime.InteropServices;

namespace AudioInjectorTest
{
    public partial class MainForm : Form
    {
        private ListBox listBoxDevices;
        private Button buttonRefresh;
        private Button buttonSelectFile;
        private Button buttonStartInjection;
        private Label labelSelectedFile;
        private Button buttonCancelInjection;
        private string selectedFilePath;

        public MainForm()
        {
            InitializeComponent();
            LoadAudioDevices();
        }

        private void InitializeComponent()
        {
            listBoxDevices = new ListBox();
            buttonRefresh = new Button();
            buttonSelectFile = new Button();
            buttonStartInjection = new Button();
            labelSelectedFile = new Label();
            buttonCancelInjection = new Button();
            SuspendLayout();
            // 
            // listBoxDevices
            // 
            listBoxDevices.FormattingEnabled = true;
            listBoxDevices.Location = new System.Drawing.Point(12, 12);
            listBoxDevices.Name = "listBoxDevices";
            listBoxDevices.Size = new System.Drawing.Size(430, 229);
            listBoxDevices.TabIndex = 0;
            // 
            // buttonRefresh
            // 
            buttonRefresh.Location = new System.Drawing.Point(462, 12);
            buttonRefresh.Name = "buttonRefresh";
            buttonRefresh.Size = new System.Drawing.Size(145, 34);
            buttonRefresh.TabIndex = 1;
            buttonRefresh.Text = "Refresh Devices";
            buttonRefresh.UseVisualStyleBackColor = true;
            buttonRefresh.Click += ButtonRefresh_Click;
            // 
            // buttonSelectFile
            // 
            buttonSelectFile.Location = new System.Drawing.Point(462, 52);
            buttonSelectFile.Name = "buttonSelectFile";
            buttonSelectFile.Size = new System.Drawing.Size(145, 34);
            buttonSelectFile.TabIndex = 2;
            buttonSelectFile.Text = "Select Audio File";
            buttonSelectFile.UseVisualStyleBackColor = true;
            buttonSelectFile.Click += ButtonSelectFile_Click;
            // 
            // buttonStartInjection
            // 
            buttonStartInjection.Location = new System.Drawing.Point(462, 167);
            buttonStartInjection.Name = "buttonStartInjection";
            buttonStartInjection.Size = new System.Drawing.Size(145, 34);
            buttonStartInjection.TabIndex = 4;
            buttonStartInjection.Text = "Start Injection";
            buttonStartInjection.UseVisualStyleBackColor = true;
            buttonStartInjection.Click += ButtonStartInjection_Click;
            // 
            // labelSelectedFile
            // 
            labelSelectedFile.AutoSize = true;
            labelSelectedFile.Location = new System.Drawing.Point(12, 256);
            labelSelectedFile.Name = "labelSelectedFile";
            labelSelectedFile.Size = new System.Drawing.Size(155, 25);
            labelSelectedFile.TabIndex = 3;
            labelSelectedFile.Text = "No audio selected";
            // 
            // buttonCancelInjection
            // 
            buttonCancelInjection.Location = new System.Drawing.Point(462, 207);
            buttonCancelInjection.Name = "buttonCancelInjection";
            buttonCancelInjection.Size = new System.Drawing.Size(145, 34);
            buttonCancelInjection.TabIndex = 5;
            buttonCancelInjection.Text = "Cancel Injection";
            buttonCancelInjection.UseVisualStyleBackColor = true;
            buttonCancelInjection.Click += ButtonCancelInjection_Click;
            // 
            // MainForm
            // 
            ClientSize = new System.Drawing.Size(623, 290);
            Controls.Add(buttonCancelInjection);
            Controls.Add(listBoxDevices);
            Controls.Add(buttonRefresh);
            Controls.Add(buttonSelectFile);
            Controls.Add(labelSelectedFile);
            Controls.Add(buttonStartInjection);
            Name = "MainForm";
            Text = "Audio Injector";
            ResumeLayout(false);
            PerformLayout();
        }

        private void LoadAudioDevices()
        {
            listBoxDevices.Items.Clear();
            try
            {
                var enumerator = new MMDeviceEnumerator();
                // Enumerate capture devices instead of render devices
                var devices = enumerator.EnumerateAudioEndPoints(DataFlow.Capture, DeviceState.Active);
                foreach (var device in devices)
                {
                    listBoxDevices.Items.Add(device.FriendlyName);
                }
                if (listBoxDevices.Items.Count > 0)
                {
                    listBoxDevices.SelectedIndex = 0;
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error loading audio devices: " + ex.Message);
            }
        }

        private void ButtonRefresh_Click(object sender, EventArgs e)
        {
            LoadAudioDevices();
        }

        private void ButtonSelectFile_Click(object sender, EventArgs e)
        {
            using OpenFileDialog ofd = new();
            ofd.Filter = "Audio Files|*.wav;*.mp3;*.aac;*.flac|All Files|*.*";
            if (ofd.ShowDialog() == DialogResult.OK)
            {
                selectedFilePath = ofd.FileName;
                labelSelectedFile.Text = selectedFilePath;
            }
        }

        private void ButtonStartInjection_Click(object sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(selectedFilePath))
            {
                MessageBox.Show("Please select an audio file first.");
                return;
            }
            if (listBoxDevices.SelectedItem == null)
            {
                MessageBox.Show("Please select a capture device.");
                return;
            }
            try
            {
                string selectedDeviceName = listBoxDevices.SelectedItem.ToString();
                // Call the AudioInjectorClient dll to inject audio to the capture device.
                int result = AudioInjectorClientWrapper.DoStartInjection(selectedDeviceName, selectedFilePath);
                if (result != 0)
                {
                    MessageBox.Show("Audio injection failed with error code: " + result);
                }
                else
                {
                    MessageBox.Show("Audio injection initiated successfully using AudioInjectorClient.");
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error during audio injection: " + ex.Message);
            }
        }

        private void ButtonCancelInjection_Click(object sender, EventArgs e)
        {
            if (listBoxDevices.SelectedItem == null)
            {
                MessageBox.Show("Please select a capture device.");
                return;
            }
            try
            {
                string selectedDeviceName = listBoxDevices.SelectedItem.ToString();
                // Call the AudioInjectorClient dll to inject audio to the capture device.
                int result = AudioInjectorClientWrapper.DoCancelInjection(selectedDeviceName);
                if (result != 0)
                {
                    MessageBox.Show("Audio cancellation failed with error code: " + result);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error during audio cancellation: " + ex.Message);
            }

        }
    }
}
