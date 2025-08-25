import os
import sys
import json
import time
import asyncio
import logging
import threading
from datetime import datetime, timezone
from typing import List, Optional, Dict, Any
from contextlib import asynccontextmanager
import socket

from fastapi import FastAPI, HTTPException, BackgroundTasks, Depends, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.encoders import jsonable_encoder
from pydantic import BaseModel, Field, field_validator, ConfigDict
import paho.mqtt.client as mqtt
from paho.mqtt import publish

# 로깅 설정
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler('openapi_server.log')
    ]
)
logger = logging.getLogger(__name__)

# 데이터 모델 정의
class SystemStatus(BaseModel):
    timestamp: int = Field(..., description="상태 정보 타임스탬프 (Unix timestamp)")
    queue_length: int = Field(0, ge=0, description="처리 대기 큐 길이")
    fps: float = Field(0.0, ge=0, description="초당 프레임 처리 수")
    saved_files: List[str] = Field(default_factory=list, description="저장된 파일 목록")
    sync_status: str = Field("unknown", description="동기화 상태 (synced/syncing/error/unknown)")
    sensor_status: str = Field("unknown", description="센서 상태 (active/inactive/error/unknown)")
    
    @field_validator('sync_status')
    @classmethod
    def validate_sync_status(cls, v):
        valid_statuses = ["synced", "syncing", "error", "unknown"]
        if v not in valid_statuses:
            raise ValueError(f"sync_status must be one of {valid_statuses}")
        return v
        
    @field_validator('sensor_status')
    @classmethod
    def validate_sensor_status(cls, v):
        valid_statuses = ["active", "inactive", "error", "unknown"]
        if v not in valid_statuses:
            raise ValueError(f"sensor_status must be one of {valid_statuses}")
        return v

class ControlCommand(BaseModel):
    action: str = Field(..., description="제어 명령 (start/stop)")
    parameters: Optional[Dict[str, Any]] = Field(
        default_factory=dict, 
        description="추가 제어 파라미터 (예: fps_limit, quality)"
    )

    @field_validator('action')
    @classmethod
    def validate_action(cls, v):
        valid_actions = ["start", "stop", "pause", "resume", "restart"]
        if v not in valid_actions:
            raise ValueError(f"action must be one of {valid_actions}")
        return v

class StatusResponse(BaseModel):
    status: str = Field(..., description="응답 상태 (success/error/no_data)")
    data: Optional[SystemStatus] = Field(default=None, description="상태 데이터")
    message: Optional[str] = Field(default=None, description="응답 메시지")

class ControlResponse(BaseModel):
    status: str = Field(..., description="응답 상태 (success/failed)")
    message: str = Field(..., description="응답 메시지")
    command_sent: bool = Field(..., description="명령 전송 성공 여부")

class HealthResponse(BaseModel):
    server_status: str
    mqtt_connected: bool
    timestamp: int
    has_status_data: bool
    uptime_seconds: float
    last_status_time: Optional[int]
    version: str

# 전역 상태 저장소
class GlobalState:
    def __init__(self):
        self.latest_status: Optional[SystemStatus] = None
        self.mqtt_client: Optional[mqtt.Client] = None
        self.mqtt_connected = False
        self.lock = threading.Lock()
        self.status_history: List[SystemStatus] = []
        self.max_history = 100  # 최대 100개 상태 기록 보관
        self.server_start_time = time.time()

    def update_status(self, status: SystemStatus):
        with self.lock:
            self.latest_status = status
            self.status_history.append(status)

            # 히스토리 크기 제한
            if len(self.status_history) > self.max_history:
                self.status_history = self.status_history[-self.max_history:]

            logger.info(f"Status updated: queue={status.queue_length}, fps={status.fps:.2f}, sync={status.sync_status}")

    def get_status(self) -> Optional[SystemStatus]:
        with self.lock:
            return self.latest_status

    def get_status_history(self, limit: int = 10) -> List[SystemStatus]:
        with self.lock:
            return self.status_history[-limit:] if self.status_history else []

    def get_uptime(self) -> float:
        return time.time() - self.server_start_time

# 전역 상태 인스턴스
global_state = GlobalState()

# MQTT Broker Settings (Using test.mosquitto.org as the default MQTT broker)
MQTT_BROKER = os.getenv("MQTT_BROKER", "test.mosquitto.org")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))  # Default MQTT port
# Generate a unique client ID with timestamp
import uuid
MQTT_CLIENT_ID = f"loki_media_player_{uuid.uuid4().hex[:8]}"

# 토픽 설정
MQTT_TOPIC_PREFIX = "loki/media"
MQTT_CONTROL_TOPIC = f"{MQTT_TOPIC_PREFIX}/control"
MQTT_STATUS_TOPIC = f"{MQTT_TOPIC_PREFIX}/status"
MQTT_COMMAND_TOPIC = f"{MQTT_TOPIC_PREFIX}/command"

# MQTT 연결 설정
MQTT_KEEPALIVE = 60
MQTT_QOS = 1
MQTT_RETAIN = True

class MQTTHandler:
    def __init__(self):
        # Initialize MQTT client with MQTT v3.1.1 for better compatibility
        self.client = mqtt.Client(
            client_id=MQTT_CLIENT_ID,
            clean_session=True,
            protocol=mqtt.MQTTv311,
            transport="tcp"  # Using TCP transport
        )
        # Configure reconnection settings
        self.client.reconnect_delay_set(min_delay=1, max_delay=30)
        # Set keepalive interval
        self.client.keepalive = MQTT_KEEPALIVE
        # Set last will message
        self.client.will_set(
            f"{MQTT_TOPIC_PREFIX}/status/offline",
            payload=json.dumps({"status": "offline", "client_id": MQTT_CLIENT_ID}),
            qos=1,
            retain=True
        )
        
        # 콜백 설정
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        self.client.on_log = self.on_log
        
        # 재연결 설정
        self.reconnect_interval = 5  # 재연결 간격 (초)
        self.reconnect_task = None
        self.connected = threading.Event()

    def on_log(self, client, userdata, level, buf):
        logger.debug(f"MQTT Log: {buf}")
        
    def publish_status(self, status_data):
        """Publish system status to MQTT"""
        if not self.client.is_connected():
            logger.warning("Cannot publish status: MQTT client is not connected")
            return False
            
        try:
            payload = json.dumps(status_data)
            result = self.client.publish(
                f"{MQTT_TOPIC_PREFIX}/status",
                payload=payload,
                qos=MQTT_QOS,
                retain=MQTT_RETAIN
            )
            if result.rc != mqtt.MQTT_ERR_SUCCESS:
                logger.error(f"Failed to publish status: {mqtt.error_string(result.rc)}")
                return False
            return True
        except Exception as e:
            logger.error(f"Error publishing status: {e}")
            return False

    def on_connect(self, client, userdata, flags, rc, properties=None):
        try:
            client_id = client._client_id.decode() if hasattr(client._client_id, 'decode') else client._client_id

            if rc == 0:
                logger.info(f"Successfully connected to MQTT broker. (Client ID: {client_id})")

                try:
                    # Subscribe to status topic
                    result, mid = client.subscribe(MQTT_STATUS_TOPIC, qos=MQTT_QOS)
                    if result == 0:
                        logger.info(f"Successfully subscribed to topic: {MQTT_STATUS_TOPIC} (QoS: {MQTT_QOS})")

                    # Subscribe to all topics under the prefix for debugging
                    result, mid = client.subscribe(f"{MQTT_TOPIC_PREFIX}/#", qos=MQTT_QOS)
                    if result == 0:
                        logger.info(f"Successfully subscribed to topic: {MQTT_TOPIC_PREFIX}/# (QoS: {MQTT_QOS})")

                    global_state.mqtt_connected = True

                    # Publish initial status
                    status_data = {
                        "status": "online",
                        "client_id": client_id,
                        "timestamp": int(time.time())
                    }
                    self.publish_status(status_data)

                except Exception as e:
                    logger.error(f"Error during subscription or status publishing: {e}")

            else:
                error_messages = {
                    1: "Invalid protocol version",
                    2: "Invalid client identifier",
                    3: "Broker unavailable",
                    4: "Bad username or password",
                    5: "Authentication failed",
                    7: "Connection refused - not authorized"
                }
                error_msg = error_messages.get(rc, f"알 수 없는 오류 (코드: {rc})")
                logger.error(f"MQTT connection failed: {error_msg}")

                # Schedule reconnection for non-fatal errors
                if rc not in [1, 2]:  # Don't retry for protocol version or client ID errors
                    logger.info("Attempting to reconnect in 5 seconds...")
                    time.sleep(5)
                    self.connect()

        except Exception as e:
            logger.error(f"Error during connection handling: {e}")

    def on_disconnect(self, client, userdata, rc, properties=None):
        global_state.mqtt_connected = False
        if rc != 0:
            logger.warning(f"MQTT broker connection unexpectedly lost. (Code: {rc})")
            self.schedule_reconnect()
        else:
            logger.info("MQTT broker connection closed normally.")

    def on_message(self, client, userdata, msg):
        try:
            topic = msg.topic
            payload = msg.payload.decode('utf-8')
            logger.debug(f"MQTT message received - Topic: {topic}, Size: {len(payload)} bytes")

            try:
                payload_data = json.loads(payload)
            except json.JSONDecodeError:
                logger.warning(f"JSON parsing failed - Topic: {topic}, Content: {payload[:100]}...")
                return

            # 상태 업데이트 메시지 처리
            if topic == MQTT_STATUS_TOPIC or topic.startswith(f"{MQTT_TOPIC_PREFIX}/status"):
                try:
                    # 타임스탬프가 없으면 현재 시간 사용
                    if 'timestamp' not in payload_data:
                        payload_data['timestamp'] = int(time.time())

                    # 필드 유효성 검사 및 기본값 설정
                    status = SystemStatus(**{
                        'timestamp': payload_data.get('timestamp', int(time.time())),
                        'queue_length': payload_data.get('queue_length', 0),
                        'fps': payload_data.get('fps', 0.0),
                        'saved_files': payload_data.get('saved_files', []),
                        'sync_status': payload_data.get('sync_status', 'unknown'),
                        'sensor_status': payload_data.get('sensor_status', 'unknown')
                    })
                    
                    global_state.update_status(status)
                    logger.info(f"Status updated: queue={status.queue_length}, fps={status.fps:.2f}")
                    
                except Exception as e:
                    logger.error(f"Error processing status update: {e}\nRaw data: {payload}")
            
            # 명령 응답 처리
            elif topic.startswith(f"{MQTT_TOPIC_PREFIX}/response/"):
                logger.info(f"Command response received: {topic} - {payload}")

        except json.JSONDecodeError as e:
            logger.error(f"MQTT message JSON parsing error: {e}, Original: {payload}")
        except Exception as e:
            logger.error(f"Error processing MQTT message: {e}")

    def schedule_reconnect(self):
        """연결이 끊겼을 때 재연결을 예약합니다."""
        if self.reconnect_task and not self.reconnect_task.done():
            return

        def reconnect():
            retry_count = 0
            max_retries = 5
            base_delay = 1  # 초 단위

            while retry_count < max_retries:
                try:
                    time.sleep(base_delay * (2 ** retry_count))  # 지수 백오프
                    self.connect()
                    logger.info("Attempting MQTT reconnection... (Attempt %d/%d)", retry_count + 1, max_retries)
                    return
                except Exception as e:
                    retry_count += 1
                    logger.error("Reconnection failed: %s (Attempt %d/%d)", e, retry_count, max_retries)

            logger.error("Maximum reconnection attempts reached.")

        # Use threading for reconnection to avoid asyncio event loop issues
        reconnect_thread = threading.Thread(target=reconnect, daemon=True)
        reconnect_thread.start()

    def connect(self):
        try:
            self.client.on_connect = self.on_connect
            self.client.on_message = self.on_message
            self.client.on_disconnect = self.on_disconnect
            self.client.on_log = self.on_log

            # Enable more detailed logging
            self.client.enable_logger(logger)

            logger.info(f"Connecting to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}...")
            
            try:
                # Set connection timeout to 10 seconds
                self.client.connect(MQTT_BROKER, MQTT_PORT, MQTT_KEEPALIVE)
                logger.info("MQTT socket connected, starting network loop...")
                
                # Start the network loop in a separate thread
                self.client.loop_start()
                
                # Wait for connection with timeout
                max_wait = 5  # Increased from 3 to 5 seconds
                for _ in range(max_wait):
                    if self.client.is_connected():
                        logger.info("MQTT client connected and loop started successfully")
                        return True
                    time.sleep(1)
                
                # If we get here, connection timed out
                logger.warning("MQTT connection timed out, but continuing in the background...")
                return False
                
            except Exception as e:
                logger.error(f"Failed to connect to MQTT broker: {str(e)}")
                logger.info("Trying alternative MQTT broker: broker.hivemq.com")
                
                # Try alternative broker
                try:
                    self.client.connect("broker.hivemq.com", 1883, MQTT_KEEPALIVE)
                    self.client.loop_start()
                    logger.info("Connected to alternative MQTT broker (broker.hivemq.com)")
                    return True
                except Exception as alt_e:
                    logger.error(f"Failed to connect to alternative broker: {str(alt_e)}")
                    return False
            
        except socket.gaierror as e:
            logger.error(f"DNS resolution failed for {MQTT_BROKER}:{MQTT_PORT}. Error: {e}")
        except socket.timeout as e:
            logger.error(f"Connection to {MQTT_BROKER}:{MQTT_PORT} timed out. Error: {e}")
        except ConnectionRefusedError as e:
            logger.error(f"Connection refused by {MQTT_BROKER}:{MQTT_PORT}. Error: {e}")
        except Exception as e:
            logger.error(f"MQTT connection error: {e}")
            
        logger.info("Will attempt to reconnect in the background...")
        return True  # Return True to allow background reconnection

    def disconnect(self):
        if self.client:
            try:
                self.client.loop_stop()
                self.client.disconnect()
                logger.info("MQTT client stopped")
            except Exception as e:
                logger.error(f"Error disconnecting MQTT: {e}")

    def send_heartbeat(self):
        """서버 생존 신호 전송"""
        try:
            heartbeat_data = {
                "server": "openapi",
                "timestamp": int(time.time()),
                "status": "alive"
            }
            message = json.dumps(heartbeat_data)
            self.client.publish("heartbeat/openapi", message, qos=0)
        except Exception as e:
            logger.error(f"Heartbeat 전송 오류: {e}")

    def publish_control_command(self, command: ControlCommand) -> bool:
        try:
            # 명령에 타임스탬프 추가
            if 'timestamp' not in command:
                command['timestamp'] = int(time.time())
                
            payload = json.dumps(command, ensure_ascii=False)
            
            # 명령 ID 생성 (중복 방지 및 추적을 위해)
            command_id = command.get('command_id', f"cmd_{int(time.time())}")
            response_topic = f"{MQTT_TOPIC_PREFIX}/response/{command_id}"
            
            # 메시지 속성 설정
            msg_properties = {
                'qos': MQTT_QOS,
                'retain': MQTT_RETAIN,
                'properties': {
                    'ResponseTopic': response_topic,
                    'CorrelationData': command_id.encode('utf-8')
                }
            }
            
            # 명령 전송
            result = self.client.publish(
                MQTT_COMMAND_TOPIC,
                payload=payload,
                **msg_properties
            )
            
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                logger.info(f"제어 명령 전송 성공: {command}")
                return True
            else:
                logger.error(f"제어 명령 전송 실패: {result}")
                return False
                
        except Exception as e:
            logger.error(f"제어 명령 전송 중 오류 발생: {e}", exc_info=True)
            return False

# MQTT 핸들러 인스턴스 생성
mqtt_handler = MQTTHandler()

# FastAPI 앱 생성 시 lifespan 이벤트 처리
@asynccontextmanager
async def lifespan(app: FastAPI):
    # 시작 시
    logger.info("Starting OpenAPI server...")
    try:
        mqtt_handler.connect()
        yield
    finally:
        # 종료 시
        logger.info("Shutting down OpenAPI server...")
        mqtt_handler.disconnect()

# FastAPI 앱 설정
app = FastAPI(
    title="Loki Media Player Control API",
    description="OpenAPI Server for Monitoring and Controlling the Loki Media Player",
    version="1.0.0",
    docs_url="/docs",
    redoc_url="/redoc",
    openapi_url="/openapi.json",
    lifespan=lifespan
)

# CORS 미들웨어 추가
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class SystemStatus(BaseModel):
    timestamp: int
    queue_length: int
    fps: float
    saved_files: List[str]
    sync_status: str
    sensor_status: str

class ControlCommand(BaseModel):
    action: str
    parameters: Dict[str, Any] = {}

class StatusResponse(BaseModel):
    status: str
    message: str
    data: SystemStatus

class ControlResponse(BaseModel):
    status: str
    message: str
    command_sent: bool

class HealthResponse(BaseModel):
    server_status: str
    mqtt_connected: bool
    timestamp: int
    has_status_data: bool
    uptime_seconds: float
    last_status_time: Optional[int]
    version: str

@app.get("/", response_model=Dict[str, Any])
async def root():
    """
    루트 엔드포인트 - 서버 기본 정보
    
    Returns:
        Dict: 서버 정보
    """
    return {
        "service": "Loki Media Player Control API",
        "version": "1.0.0",
        "documentation": "/docs",
        "status": "running",
        "mqtt_connected": mqtt_handler.connected.is_set(),
        "timestamp": int(time.time()),
        "endpoints": [
            {"path": "/status", "method": "GET", "description": "시스템 상태 조회"},
            {"path": "/control", "method": "POST", "description": "시스템 제어"},
            {"path": "/health", "method": "GET", "description": "상태 확인"}
        ]
    }

@app.post("/status", response_model=StatusResponse, status_code=status.HTTP_202_ACCEPTED)
async def receive_status(status_data: SystemStatus):
    """
    수집 시스템에서 전달되는 상태 정보 수신 및 저장
    
    - **timestamp**: Unix timestamp 형태의 상태 정보 시각
    - **queue_length**: 처리 대기중인 작업 수
    - **fps**: 초당 처리 프레임 수
    - **saved_files**: 저장된 파일 경로 목록
    - **sync_status**: 동기화 상태 (synced/syncing/error/unknown)
    - **sensor_status**: 센서 상태 (active/inactive/error/unknown)
    
    Returns:
        StatusResponse: 처리 결과
    """
    try:
        # 상태 업데이트
        global_state.update_status(status_data)
        
        # MQTT로도 상태 브로드캐스트 (선택사항)
        if mqtt_handler.connected.is_set():
            mqtt_handler.client.publish(
                MQTT_STATUS_TOPIC,
                payload=status_data.json(),
                qos=MQTT_QOS,
                retain=MQTT_RETAIN
            )
        
        return {
            "status": "success",
            "data": status_data,
            "message": "Status updated successfully"
        }
    except Exception as e:
        logger.error(f"상태 업데이트 오류: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail={"status": "error", "message": str(e)}
        )

@app.get("/status", response_model=StatusResponse)
async def get_status():
    """
    최신 수집 시스템 상태 조회
    
    현재 저장된 가장 최신의 시스템 상태를 반환합니다.
    
    Returns:
        StatusResponse: 시스템 상태 정보
    """
    try:
        status = global_state.get_status()
        if status is None:
            # 기본 상태 생성
            status = SystemStatus(
                timestamp=int(time.time()),
                queue_length=0,
                fps=0.0,
                saved_files=[],
                sync_status="unknown",
                sensor_status="unknown"
            )
            
        return {
            "status": "success",
            "data": status,
            "message": "Latest status retrieved successfully"
        }
    except Exception as e:
        logger.error(f"상태 조회 중 오류: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail={"status": "error", "message": str(e)}
        )

@app.post("/control", response_model=ControlResponse, status_code=status.HTTP_202_ACCEPTED)
async def send_control_command(
    command: ControlCommand,
    background_tasks: BackgroundTasks
):
    """
    수집 시스템 제어 명령 전송
    
    - **action**: 제어 명령 (start/stop)
    - **parameters**: 추가 제어 파라미터 (선택사항)
    
    명령은 MQTT를 통해 수집 시스템으로 전달됩니다.
    
    Returns:
        ControlResponse: 명령 처리 결과
    """
    try:
        command_dict = command.dict()
        
        # 백그라운드에서 명령 전송 (비동기 처리)
        background_tasks.add_task(
            mqtt_handler.send_control_command,
            command_dict
        )
        
        logger.info(f"제어 명령 수신: {command.action} (parameters: {command.parameters})")
        
        return {
            "status": "success",
            "message": f"Command '{command.action}' is being processed",
            "command_sent": True
        }
        
    except Exception as e:
        logger.error(f"제어 명령 처리 중 오류: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail={
                "status": "error",
                "message": f"Failed to process command: {str(e)}",
                "command_sent": False
            }
        )

@app.get("/health", response_model=HealthResponse)
async def health_check():
    """
    서버 및 MQTT 연결 상태 확인
    
    시스템의 전반적인 건강 상태를 확인할 수 있습니다.
    
    Returns:
        HealthResponse: 시스템 상태 정보
    """
    try:
        mqtt_connected = mqtt_handler.connected.is_set()
        latest_status = global_state.get_status()
        
        # MQTT 연결 상태 로깅 (디버그용)
        if not mqtt_connected:
            logger.warning("MQTT 연결이 끊어졌습니다.")
        
        return {
            "server_status": "running",
            "mqtt_connected": mqtt_connected,
            "timestamp": int(time.time()),
            "has_status_data": latest_status is not None,
            "uptime_seconds": global_state.get_uptime(),
            "last_status_time": latest_status.timestamp if latest_status else None,
            "version": "1.0.0"
        }
    except Exception as e:
        logger.error(f"상태 확인 중 오류: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail={
                "server_status": "error",
                "error": str(e)
            }
        )

# 추가 유용한 엔드포인트들
@app.get("/files", tags=["파일 관리"])
async def get_saved_files():
    """
    저장된 파일 목록 조회
    """
    latest_status = global_state.get_status()
    if latest_status is None:
        return {"files": [], "message": "상태 정보가 없습니다.", "count": 0}

    return {
        "files": latest_status.saved_files,
        "count": len(latest_status.saved_files),
        "timestamp": latest_status.timestamp,
        "last_updated": datetime.fromtimestamp(latest_status.timestamp, tz=datetime.timezone.utc).isoformat()
    }

@app.get("/metrics", tags=["메트릭"])
async def get_metrics():
    """
    시스템 메트릭 조회
    """
    latest_status = global_state.get_status()
    if latest_status is None:
        raise HTTPException(status_code=404, detail="상태 정보가 없습니다.")

    return {
        "current_metrics": {
            "fps": latest_status.fps,
            "queue_length": latest_status.queue_length,
            "sync_status": latest_status.sync_status,
            "sensor_status": latest_status.sensor_status,
            "file_count": len(latest_status.saved_files),
        },
        "system_info": {
            "last_updated": latest_status.timestamp,
            "server_uptime": global_state.get_uptime(),
            "mqtt_connected": mqtt_handler.connected.is_set()
        }
    }

@app.get("/status/history", tags=["상태 관리"])
async def get_status_history(limit: int = 10):
    """
    상태 히스토리 조회

    - **limit**: 반환할 히스토리 개수 (기본값: 10, 최대: 100)
    """
    if limit > 100:
        limit = 100

    history = global_state.get_status_history(limit)

    return {
        "history": history,
        "count": len(history),
        "limit": limit
    }

def main():
    import uvicorn
    import argparse
    
    # 명령줄 인수 파싱
    parser = argparse.ArgumentParser(description='Loki Media Player OpenAPI 서버')
    parser.add_argument('--host', type=str, default='0.0.0.0', help='호스트 주소')
    parser.add_argument('--port', type=int, default=8000, help='포트 번호')
    parser.add_argument('--reload', action='store_true', help='자동 리로드 활성화')
    parser.add_argument('--log-level', type=str, default='info', 
                       choices=['debug', 'info', 'warning', 'error', 'critical'],
                       help='로그 레벨')
    
    args = parser.parse_args()
    
    # 서버 시작 메시지
    logger.info(f"Starting Loki Media Player OpenAPI server... (http://{args.host}:{args.port})")
    logger.info(f"MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
    logger.info(f"Log level: {args.log_level.upper()}")
    
    # MQTT 연결 시작
    try:
        mqtt_handler.connect()
    except Exception as e:
        logger.error(f"MQTT 연결 실패: {e}")
    
    # FastAPI 서버 시작
    uvicorn.run(
        "openapi_server:app",
        host=args.host,
        port=args.port,
        reload=args.reload,
        log_level=args.log_level,
        access_log=True,
        use_colors=True
    )

if __name__ == "__main__":
    main()