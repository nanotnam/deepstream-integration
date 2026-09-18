FROM python:3.13-slim

WORKDIR /opt/mbfs/event-forwarder
COPY services/event-forwarder/ ./
COPY contracts/ /opt/mbfs/contracts/
RUN pip install --no-cache-dir .

RUN useradd --create-home --uid 10001 forwarder
USER forwarder
ENV EVENT_FORWARDER_CONTRACT_ROOT=/opt/mbfs/contracts
EXPOSE 8080
HEALTHCHECK --interval=10s --timeout=3s --retries=3 \
  CMD ["python", "-c", "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8080/live', timeout=2)"]
ENTRYPOINT ["event-forwarder"]
