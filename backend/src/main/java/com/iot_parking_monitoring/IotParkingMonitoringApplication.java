package com.iot_parking_monitoring;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import com.iot_parking_monitoring.receivers.Receiver;
import io.github.cdimascio.dotenv.Dotenv;

@SpringBootApplication
public class IotParkingMonitoringApplication {

	public static void main(String[] args) {
        Dotenv dotenv = Dotenv.load();
        String connectionString = dotenv.get("EVENT_HUB_CONNECTION_STRING");
        String eventHubName = dotenv.get("EVENT_HUB_NAME");

        Receiver receiver = new Receiver(connectionString, eventHubName);
        receiver.start();
		SpringApplication.run(IotParkingMonitoringApplication.class, args);
	}

}