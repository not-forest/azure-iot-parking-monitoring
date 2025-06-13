FROM openjdk:21-jdk
WORKDIR /app
COPY target/iot_parking_monitoring-0.0.1-SNAPSHOT.jar app.jar
COPY .env .env
ENTRYPOINT ["java", "-jar", "app.jar"]