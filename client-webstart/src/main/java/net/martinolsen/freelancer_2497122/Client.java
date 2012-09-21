package net.martinolsen.freelancer_2497122;

import java.io.*;
import java.net.*;

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.border.*;
import javax.swing.event.*;
import javax.swing.text.*;

public class Client extends JFrame {
    private MainArea mainArea = new MainArea();
    private StatusBar statusBar = new StatusBar();

    public Client() {
        add(mainArea, BorderLayout.CENTER);
        add(statusBar, BorderLayout.SOUTH);

        setSize(350, 300);
        setTitle("Client");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLocationRelativeTo(null);
    }

    private class MainArea extends JPanel {
        private ServerPanel serverPanel = new ServerPanel();
        private MessagesComponent messagesComponent = new MessagesComponent();
        private MessagePanel messagePanel = new MessagePanel();

        public MainArea() {
            setLayout(new BorderLayout());

            add(serverPanel, BorderLayout.NORTH);
            add(messagesComponent, BorderLayout.CENTER);
            add(messagePanel, BorderLayout.SOUTH);
        }
    }

    private class StatusBar extends JPanel {
        private JLabel label = new JLabel("status");

        public StatusBar() {
            setBorder(new BevelBorder(BevelBorder.LOWERED));
            setLayout(new BoxLayout(this, BoxLayout.X_AXIS));

            add(label);
        }

        public JLabel getLabel() {
            return this.label;
        }
    }

    private class ServerPanel extends JPanel {
        public ServerPanel() {
            setLayout(new BorderLayout());

            JLabel serverLabel = new JLabel("Server");

            JTextField serverTextField = new JTextField();
            serverTextField.getDocument().addDocumentListener(new ServerTextFieldDocumentListener());

            add(serverLabel, BorderLayout.LINE_START);
            add(serverTextField, BorderLayout.CENTER);
        }
    }

    private class MessagesComponent extends JTextArea {
        public MessagesComponent() {
            super();
        }
    }

    private class MessagePanel extends JPanel {
        public MessagePanel() {
            JButton sendButton = new JButton("Send");
            sendButton.addActionListener(new ActionListener() {
                public void actionPerformed(ActionEvent ae) {
                    System.out.println("SEND!");
                }
            });

            JTextField messageTextField = new JTextField();

            setLayout(new BorderLayout());

            add(sendButton, BorderLayout.LINE_START);
            add(messageTextField, BorderLayout.CENTER);
        }
    }

    private void setConnection(String host) {
        System.out.println("Connect: " + host + "!");

        // Stop the listener and writer, create new, overwrite old
    }

    private LineBufferedSocketReader reader = null;
    private Thread readerThread = null;

    private void setStatus(String text) {
        this.statusBar.getLabel().setText(text);
    }

    private void setConnection(String host, Integer port) {
        Socket socket = null;

        try {
            socket = new Socket(host, port);
        } catch(IOException ex) {
            setStatus("could not connect to " + host + ": " + ex.getMessage());
        }

        if(socket != null)
            setConnection(socket);
    }

    private void setConnection(Socket socket) {
        reader = new LineBufferedSocketReader(socket);
        readerThread = new Thread(reader);
        readerThread.run();

        setStatus("connected to " + socket.getInetAddress().getHostName() + ":" + socket.getPort());
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(new Runnable() {
            public void run() {
                Client client = new Client();
                client.setVisible(true);
                client.setConnection("gopher.local", 10001);
            }
        });
    }

    private class ServerTextFieldDocumentListener implements DocumentListener {
        public void changedUpdate(DocumentEvent documentEvent) {
            addressUpdate(documentEvent.getDocument());
        }

        public void insertUpdate(DocumentEvent documentEvent) {
            addressUpdate(documentEvent.getDocument());
        }

        public void removeUpdate(DocumentEvent documentEvent) {
            addressUpdate(documentEvent.getDocument());
        }

        private void addressUpdate(Document document) {
            String text = null;

            try {
                text = document.getText(0, document.getLength());
            } catch(BadLocationException ex) {
                ex.printStackTrace();
            }

            String address = parseAndValidateAddressText(text);

            setConnection(address);
        }

        private String parseAndValidateAddressText(String text) {
            return text.trim();
        }
    }

    private class LineBufferedSocketReader implements Runnable {
        private Socket socket;

        public LineBufferedSocketReader(Socket socket) {
            this.socket = socket;
        }

        public void run() {
            // read from socket, write to line buffer
            // (the client should the read from its buffer, line by line)
        }

        public void setSocket(Socket socket) {
            this.socket = socket;
        }

        public Socket getSocket() {
            return this.socket;
        }
    }

    private class LineBufferedSocketWriter implements Runnable {
        private Socket socket;

        public void run() {
        }

        public void setSocket(Socket socket) {
            this.socket = socket;
        }

        public Socket getSocket() {
            return this.socket;
        }
    }
}
