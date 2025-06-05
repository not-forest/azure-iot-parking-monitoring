package com.iot_parking_monitoring.receivers;

import java.time.Instant;
import com.azure.messaging.eventhubs.*;
import com.azure.messaging.eventhubs.models.*;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
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

        ObjectMapper mapper = new ObjectMapper();
        
        consumer.getPartitionIds().subscribe(partitionId -> {
            consumer.receiveFromPartition(partitionId, EventPosition.latest())
                .subscribe(partitionEvent -> {
                    String eventBody = partitionEvent.getData().getBodyAsString();
                    Instant enqueuedTime = partitionEvent.getData().getEnqueuedTime();
                    try {
                        ObjectNode json = mapper.createObjectNode();
                        json.put("time", enqueuedTime.toString());
                        JsonNode dataNode = mapper.readTree(eventBody);
                        json.set("data", dataNode);
                        String messageWithTime = mapper.writeValueAsString(json);
                        System.out.printf("Received event from partition %s: %s%n", partitionId, messageWithTime);
                        webSocketController.sendMessageToClients(messageWithTime);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                });
        });

        System.out.println("Receiving events... Press Ctrl+C to exit.");
    }
}
