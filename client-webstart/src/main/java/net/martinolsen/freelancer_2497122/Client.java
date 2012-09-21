package net.martinolsen.freelancer_2497122;

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.border.*;
import javax.swing.event.*;
import javax.swing.text.*;

public class Client extends JFrame {
    public Client() {
        add(new MainArea(), BorderLayout.CENTER);
        //add(new StatusBar(), BorderLayout.SOUTH);

        setSize(350, 300);
        setTitle("Client");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLocationRelativeTo(null);
    }

    private class MainArea extends JPanel {
        private MainArea() {
            add(initServerPanel(), BorderLayout.NORTH);
            add(initMessagesComponent(), BorderLayout.CENTER);
            add(initMessageComponent(), BorderLayout.SOUTH);
        }
    }

    private class StatusBar extends JPanel {
        private JLabel label = new JLabel("status");

        private StatusBar() {
            add(label);

            setBorder(new BevelBorder(BevelBorder.LOWERED));
            setLayout(new BoxLayout(this, BoxLayout.X_AXIS));
        }
    }

    private JComponent initServerPanel() {
        JLabel serverLabel = new JLabel("Server");

        JTextField serverTextField = new JTextField();
        serverTextField.getDocument().addDocumentListener(new ServerTextFieldDocumentListener());

        JPanel panel = new JPanel(new BorderLayout());
        panel.add(serverLabel, BorderLayout.LINE_START);
        panel.add(serverTextField, BorderLayout.CENTER);

        return panel;
    }

    private JComponent initMessagesComponent() {
        return new JTextArea();
    }

    private JComponent initMessageComponent() {
        JButton sendButton = new JButton("Send");
        sendButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent ae) {
                System.out.println("SEND!");
            }
        });

        JTextField messageTextField = new JTextField();

        JPanel panel = new JPanel(new BorderLayout());

        panel.add(sendButton, BorderLayout.LINE_START);
        panel.add(messageTextField, BorderLayout.CENTER);

        return panel;
    }

    private void setConnection(String host) {
        System.out.println("Connect: " + host + "!");
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(new Runnable() {
            public void run() {
                Client client = new Client();
                client.setVisible(true);
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
}
