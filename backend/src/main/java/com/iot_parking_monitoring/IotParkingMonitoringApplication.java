package com.iot_parking_monitoring;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.ApplicationContext;
import com.iot_parking_monitoring.receivers.Receiver;
import com.iot_parking_monitoring.controllers.WebSocketController;

@SpringBootApplication
public class IotParkingMonitoringApplication {
    public static void main(String[] args) {
        ApplicationContext context = SpringApplication.run(IotParkingMonitoringApplication.class, args);

        String connectionString = System.getenv("EVENT_HUB_CONNECTION_STRING");
        String eventHubName = System.getenv("EVENT_HUB_NAME");
        WebSocketController webSocketController = context.getBean(WebSocketController.class);
        Receiver receiver = new Receiver(connectionString, eventHubName, webSocketController);
        receiver.start();
    }
}