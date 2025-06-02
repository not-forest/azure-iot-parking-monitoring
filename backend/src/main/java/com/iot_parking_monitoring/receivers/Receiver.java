package com.iot_parking_monitoring.receivers;

import com.azure.messaging.eventhubs.*;
import com.azure.messaging.eventhubs.models.*;
import com.iot_parking_monitoring.configurations.WebSocketHandler;

public class Receiver {

    private final String connectionString;
    private final String eventHubName;
    private final WebSocketHandler webSocketHandler;

    public Receiver(String connectionString, String eventHubName, WebSocketHandler webSocketHandler) {
        this.connectionString = connectionString;
        this.eventHubName = eventHubName;
        this.webSocketHandler = webSocketHandler;
    }

    public void start() {
        EventHubConsumerAsyncClient consumer = new EventHubClientBuilder()
            .connectionString(connectionString, eventHubName)
            .consumerGroup(EventHubClientBuilder.DEFAULT_CONSUMER_GROUP_NAME)
            .buildAsyncConsumerClient();

        consumer.getPartitionIds().subscribe(partitionId -> {
            consumer.receiveFromPartition(partitionId, EventPosition.latest())
                .subscribe(partitionEvent -> {
                    String body = partitionEvent.getData().getBodyAsString();
                    System.out.printf("Received event from partition %s: %s%n",
                        partitionId,
                        partitionEvent.getData().getBodyAsString());
                        webSocketHandler.sendMessageToAll(body);
                });
        });

        System.out.println("Receiving events... Press Ctrl+C to exit.");
    }
}
