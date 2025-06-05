package com.iot_parking_monitoring;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.ApplicationContext;
import com.iot_parking_monitoring.receivers.Receiver;
import io.github.cdimascio.dotenv.Dotenv;
import com.iot_parking_monitoring.controllers.WebSocketController;

@SpringBootApplication
public class IotParkingMonitoringApplication {
    public static void main(String[] args) {
        ApplicationContext context = SpringApplication.run(IotParkingMonitoringApplication.class, args);

        Dotenv dotenv = Dotenv.load();
        String connectionString = dotenv.get("EVENT_HUB_CONNECTION_STRING");
        String eventHubName = dotenv.get("EVENT_HUB_NAME");
        WebSocketController webSocketController = context.getBean(WebSocketController.class);
        Receiver receiver = new Receiver(connectionString, eventHubName, webSocketController);
        receiver.start();
    }
}