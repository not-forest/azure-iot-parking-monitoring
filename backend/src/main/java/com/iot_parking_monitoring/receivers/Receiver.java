package com.iot_parking_monitoring.receivers;

import com.azure.messaging.eventhubs.*;
import com.azure.messaging.eventhubs.models.*;

public class Receiver {

    private final String connectionString;
    private final String eventHubName;

    public Receiver(String connectionString, String eventHubName) {
        this.connectionString = connectionString;
        this.eventHubName = eventHubName;
    }

    public void start() {
        EventHubConsumerAsyncClient consumer = new EventHubClientBuilder()
            .connectionString(connectionString, eventHubName)
            .consumerGroup(EventHubClientBuilder.DEFAULT_CONSUMER_GROUP_NAME)
            .buildAsyncConsumerClient();

        consumer.getPartitionIds().subscribe(partitionId -> {
            consumer.receiveFromPartition(partitionId, EventPosition.latest())
                .subscribe(partitionEvent -> {
                    System.out.printf("Received event from partition %s: %s%n",
                        partitionId,
                        partitionEvent.getData().getBodyAsString());
                });
        });

        System.out.println("Receiving events... Press Ctrl+C to exit.");
    }
}
