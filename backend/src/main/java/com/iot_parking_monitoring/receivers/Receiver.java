package com.iot_parking_monitoring.receivers;

import com.azure.messaging.eventhubs.*;
import com.azure.messaging.eventhubs.models.*;
import com.iot_parking_monitoring.configurations.WebSocketHandler;
import com.iot_parking_monitoring.controllers.WebSocketController;

public class Receiver {

    private final String connectionString;
    private final String eventHubName;
    private final WebSocketController webSocketController;

    public Receiver(String connectionString, String eventHubName, WebSocketController webSocketController) {
        this.connectionString = connectionString;
        this.eventHubName = eventHubName;
        this.webSocketController = webSocketController;
    }

    public void start() {
        EventHubConsumerAsyncClient consumer = new EventHubClientBuilder()
            .connectionString(connectionString, eventHubName)
            .consumerGroup(EventHubClientBuilder.DEFAULT_CONSUMER_GROUP_NAME)
            .buildAsyncConsumerClient();

        consumer.getPartitionIds().subscribe(partitionId -> {
            consumer.receiveFromPartition(partitionId, EventPosition.latest())
                .subscribe(partitionEvent -> {
                    String eventBody = partitionEvent.getData().getBodyAsString();
                    System.out.printf("Received event from partition %s: %s%n", partitionId, eventBody);
                    try {
                        webSocketController.sendMessageToClients(eventBody);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                });
        });

        System.out.println("Receiving events... Press Ctrl+C to exit.");
    }
}
