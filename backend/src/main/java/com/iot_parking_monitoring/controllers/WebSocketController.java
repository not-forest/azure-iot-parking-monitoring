package com.iot_parking_monitoring.controllers;

import org.springframework.messaging.handler.annotation.MessageMapping;
import org.springframework.messaging.handler.annotation.SendTo;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Controller;

@Controller
public class WebSocketController {

    private final SimpMessagingTemplate messagingTemplate;

    public WebSocketController(SimpMessagingTemplate messagingTemplate) {
        this.messagingTemplate = messagingTemplate;
    }

    public void sendMessageToClients(String message) {
        messagingTemplate.convertAndSend("/topic/parking", message);
    }

    @MessageMapping("/changeSpotStatus")
    @SendTo("/topic/parking")
    public String handleSpotStatusChange(String message) {
        return message;
    }
}
