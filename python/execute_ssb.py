import sys, pathlib
root = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(root))            # so `generated` is importable
sys.path.insert(0, str(root / "generated"))  # so `QueryPlan_pb2` (top-level) resolves

import inspect

import threading
import datetime
import argparse

import msg
import client as tcp_client
import util
from generated import WorkResponse_pb2 as WorkResponse
from generated import NetworkRequests_pb2 as NetworkRequests
from generated import WorkItem_pb2 as WorkItem
from generated import QueryPlan_pb2 as QueryPlan

name_list = []
uuid_list = []
complete_tasks = []
query_bodies = None
uuid_condition = threading.Condition()
complete_condition = threading.Condition()
# The `update_uuids` function is a callback function that is used to update the pairs for Unit
# name and their registered UUID. It takes a `TCPMessage` object as input, extracts the payload
# from the message, and then parses it into a `UuidForUnitResponse` object from the
# `NetworkRequests` protocol buffer.
def update_uuids(message: msg.TCPMessage):
    global name_list, uuid_list, uuid_condition
    response = NetworkRequests.UuidForUnitResponse()
    response.ParseFromString(message.payload)
    with uuid_condition:
        for name in response.names:
            name_list.append(name)
        for uuid in response.uuids:
            uuid_list.append(uuid)
        uuid_condition.notify_all()


# The `task_finished_cb` function is a callback function that is called when a task responds with its
# finished status. It takes a `TCPMessage` object as input, extracts the payload from the message, and
# then parses it into a `WorkResponse` object from the `WorkResponse` protocol buffer.
def task_finished_cb(message: msg.TCPMessage):
    global complete_condition, complete_tasks
    response = WorkResponse.WorkResponse()
    response.ParseFromString(message.payload)
    with complete_condition:
        # print(f"Received response: {response}")
        complete_tasks.append(f"{response.planId}_{response.itemId}")
        complete_condition.notify_all()


# The `get_timedelta` function calculates the time difference between two datetime objects `t_s`
# (start time) and `t_e` (end time). It returns a string representation of the time delta in seconds
# and microseconds. The function prints the time delta in seconds and microseconds before returning
# the total time in microseconds as a string.
def get_timedelta(t_s: datetime, t_e: datetime) -> str:
    delta = t_e - t_s
    delta_str = f"{delta.seconds} s {delta.microseconds} us"
    print(delta_str)
    return str(delta.seconds*1000000 + delta.microseconds)


def run_query(client: tcp_client.TCPClient, query: str) -> int:
    global query_bodies
    query: QueryPlan = query_bodies[query]()
    t_s = datetime.datetime.now()
    client.send_message(query)
    t_e = datetime.datetime.now()
    return get_timedelta(t_s, t_e)


def q_1_1(planId: int = 1) -> msg.TCPMessage:
    workItems = []
    workItems.append(util.create_work_item(itemType="int_filter",
        planId=planId, itemId=1, operatorId=WorkItem.OP_FILTER,
        inputTable="lineorder", inputColumn="lo_discount", inputType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_1", outputType=WorkItem.TYPE_BITMASK,
        filterType=WorkItem.COMP_BETWEEN, filterArgVals=[1, 3]))

    workItems.append(util.create_work_item(itemType="int_filter",
        planId=planId, itemId=2, operatorId=WorkItem.OP_FILTER,
        inputTable="lineorder", inputColumn="lo_quantity", inputType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_2", outputType=WorkItem.TYPE_BITMASK,
        filterType=WorkItem.COMP_LT, filterArgVals=[25]))

    workItems.append(util.create_work_item(itemType="int_filter",
        planId=planId, itemId=3, operatorId=WorkItem.OP_FILTER,
        inputTable="dates", inputColumn="d_year", inputType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_3", outputType=WorkItem.TYPE_BITMASK,
        filterType=WorkItem.COMP_EQ, filterArgVals=[1993]))

    workItems.append(util.create_work_item(itemType="set_operation",
        planId=planId, itemId=4, dependsOn=[1, 2], operatorId=WorkItem.OP_SETOPERATION,
        operation=WorkItem.REL_INTERSECTION,
        innerTable="intermediate", innerColumn="q_1_1_it_1", innerType=WorkItem.TYPE_BITMASK,
        outerTable="intermediate", outerColumn="q_1_1_it_2", outerType=WorkItem.TYPE_BITMASK,
        outputTable="intermediate", outputColumn="q_1_1_it_4", outputType=WorkItem.TYPE_BITMASK))

    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=5, dependsOn=[4], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_1_it_4", indexType=WorkItem.TYPE_BITMASK,
        filterTable="lineorder", filterColumn="lo_orderdate", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_5", outputType=WorkItem.TYPE_INTEGER))
    
    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=6, dependsOn=[3], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_1_it_3", indexType=WorkItem.TYPE_BITMASK,
        filterTable="dates", filterColumn="d_datekey", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_6", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="join",
        planId=planId, itemId=7, dependsOn=[5, 6], operatorId=WorkItem.OP_HASHJOIN,
        innerTable="intermediate", innerColumn="q_1_1_it_6", innerType=WorkItem.TYPE_INTEGER,
        outerTable="intermediate", outerColumn="q_1_1_it_5", outerType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_7", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=8, dependsOn=[4], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_1_it_4", indexType=WorkItem.TYPE_BITMASK,
        filterTable="lineorder", filterColumn="lo_extendedprice", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_8", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=9, dependsOn=[4], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_1_it_4", indexType=WorkItem.TYPE_BITMASK,
        filterTable="lineorder", filterColumn="lo_discount", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_9", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=10, dependsOn=[7, 8], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_1_it_7_o", indexType=WorkItem.TYPE_BITMASK,
        filterTable="intermediate", filterColumn="q_1_1_it_8", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_10", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=11, dependsOn=[7, 9], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_1_it_7_o", indexType=WorkItem.TYPE_BITMASK,
        filterTable="intermediate", filterColumn="q_1_1_it_9", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_11", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="map",
        planId=planId, itemId=12, dependsOn=[10, 11], operatorId=WorkItem.OP_MAP, operatorType=WorkItem.ARITH_MUL,
        inputTable="intermediate", inputColumn="q_1_1_it_10", inputType=WorkItem.TYPE_BITMASK,
        partnerTable="intermediate", partnerColumn="q_1_1_it_11", partnerType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_12", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="aggregate",
        planId=planId, itemId=13, dependsOn=[12], operatorId=WorkItem.OP_AGGREGATE, aggregationFunction=WorkItem.AGG_SUM,
        inputTable="intermediate", inputColumn="q_1_1_it_12", inputType=WorkItem.TYPE_BITMASK,
        outputTable="result", outputColumn="q_1_1_it_13", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="result", 
        planId=planId, itemId=14, dependsOn=[13],
        resultTables=["result"], resultColumns=["q_1_1_it_13"], resultHeader=["revenue"], resultName="result_q_1_1"))
    
    return util.create_query_plan(planId=planId, workItems=workItems)


def q_1_2(planId: int = 2) -> msg.TCPMessage:
    workItems = []
    workItems.append(util.create_work_item(itemType="int_filter",
        planId=planId, itemId=1, operatorId=WorkItem.OP_FILTER,
        inputTable="lineorder", inputColumn="lo_discount", inputType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_2_it_1", outputType=WorkItem.TYPE_BITMASK,
        filterType=WorkItem.COMP_BETWEEN, filterArgVals=[4, 6]))

    workItems.append(util.create_work_item(itemType="int_filter",
        planId=planId, itemId=2, operatorId=WorkItem.OP_FILTER,
        inputTable="lineorder", inputColumn="lo_quantity", inputType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_2_it_2", outputType=WorkItem.TYPE_BITMASK,
        filterType=WorkItem.COMP_BETWEEN, filterArgVals=[26, 35]))

    workItems.append(util.create_work_item(itemType="string_filter",
        planId=planId, itemId=3, operatorId=WorkItem.OP_FILTER,
        inputTable="dates", inputColumn="d_yearmonth", inputType=WorkItem.TYPE_STRING,
        outputTable="intermediate", outputColumn="q_1_2_it_3", outputType=WorkItem.TYPE_BITMASK,
        filterType=WorkItem.COMP_EQ, filterArgVals=["Jan1994"]))

    workItems.append(util.create_work_item(itemType="set_operation",
        planId=planId, itemId=4, dependsOn=[1, 2], operatorId=WorkItem.OP_SETOPERATION,
        operation=WorkItem.REL_INTERSECTION,
        innerTable="intermediate", innerColumn="q_1_2_it_1", innerType=WorkItem.TYPE_BITMASK,
        outerTable="intermediate", outerColumn="q_1_2_it_2", outerType=WorkItem.TYPE_BITMASK,
        outputTable="intermediate", outputColumn="q_1_2_it_4", outputType=WorkItem.TYPE_BITMASK))
    
    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=5, dependsOn=[4], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_2_it_4", indexType=WorkItem.TYPE_BITMASK,
        filterTable="lineorder", filterColumn="lo_orderdate", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_2_it_5", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=6, dependsOn=[3], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_2_it_3", indexType=WorkItem.TYPE_BITMASK,
        filterTable="dates", filterColumn="d_datekey", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_2_it_6", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="join",
        planId=planId, itemId=7, dependsOn=[5, 6], operatorId=WorkItem.OP_HASHJOIN,
        innerTable="intermediate", innerColumn="q_1_2_it_6", innerType=WorkItem.TYPE_INTEGER,
        outerTable="intermediate", outerColumn="q_1_2_it_5", outerType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_2_it_7", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="matrialize",
        planId=planId, itemId=8, dependsOn=[4], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_2_it_4", indexType=WorkItem.TYPE_BITMASK,
        filterTable="lineorder", filterColumn="lo_extendedprice", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_2_it_8", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=9, dependsOn=[4], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_2_it_4", indexType=WorkItem.TYPE_BITMASK,
        filterTable="lineorder", filterColumn="lo_discount", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_2_it_9", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=10, dependsOn=[7, 8], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_2_it_7_o", indexType=WorkItem.TYPE_BITMASK,
        filterTable="intermediate", filterColumn="q_1_2_it_8", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_2_it_10", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="materialize",
        planId=planId, itemId=11, dependsOn=[7, 9], operatorId=WorkItem.OP_MATERIALIZE,
        indexTable="intermediate", indexColumn="q_1_2_it_7_o", indexType=WorkItem.TYPE_BITMASK,
        filterTable="intermediate", filterColumn="q_1_2_it_9", filterType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_2_it_11", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="map",
        planId=planId, itemId=12, dependsOn=[10, 11], operatorId=WorkItem.OP_MAP, operatorType=WorkItem.ARITH_MUL,
        inputTable="intermediate", inputColumn="q_1_2_it_10", inputType=WorkItem.TYPE_BITMASK,
        partnerTable="intermediate", partnerColumn="q_1_2_it_11", partnerType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_2_it_12", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="aggregate",
        planId=planId, itemId=13, dependsOn=[12], operatorId=WorkItem.OP_AGGREGATE, aggregationFunction=WorkItem.AGG_SUM,
        inputTable="intermediate", inputColumn="q_1_2_it_12", inputType=WorkItem.TYPE_BITMASK,
        outputTable="result", outputColumn="q_1_2_it_13", outputType=WorkItem.TYPE_INTEGER))

    workItems.append(util.create_work_item(itemType="result",
        planId=planId, itemId=14, dependsOn=[13],
        resultTables=["result"], resultColumns=["q_1_2_it_13"], resultHeader=["revenue"],resultName="result_q_1_2"))
    
    return util.create_query_plan(planId=planId, workItems=workItems)
    

def execute_ssb(client: tcp_client.TCPClient, q_parameter: list):
    global query_bodies
    # collect all query functions defined in this module whose names start with "q_"
    query_bodies = {}
    current_module = sys.modules[__name__]
    for name, func in inspect.getmembers(current_module, inspect.isfunction):
        if name.startswith("q_"):
            query_bodies[name] = func
    
    client.register_callback(msg.TCPPackageType.TaskFinished, task_finished_cb)

    """ Fetch all UUIDs for all units """
    work = util.create_uuid_request_item(type=msg.UnitType.ComputeUnit)
    client.send_message(work)
    with uuid_condition:
        if len(uuid_list) == 0 and client.connection_up:
            uuid_condition.wait(0.1)

    """ Select the first ComputeUnit that we find """
    tgt_uuid = 0
    for name, uuid in zip(name_list, uuid_list):
        if "ComputeUnit" in name:
            tgt_uuid = uuid
            break

    # if tgt_uuid == 0:
    #     print("[Error] Could not find a ComputeUnit. Exiting.")
    #     client.disconnect()
    #     exit(-1)

    deltas = []
    if q_parameter:
        for q in q_parameter:
            deltas.append(run_query(client, q))
            complete_tasks.clear()
        
        for q, d in zip(q_parameter, deltas):
            print(f"Time taken for {q}: {d} us")
    else:
        print("Running all SSB Queries.")
        for query in ["q_1_1", "q_1_2"]:
            deltas.append(run_query(client, query))
    
    client.disconnect()
    
    
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-ip', help='The IP of the message bus',
                        default="127.0.0.1", required=False)
    parser.add_argument('-port', help='The port of the message bus',
                        type=int, default=23232, required=False)
    parser.add_argument('-info', help='Some meta info about this units purpose.',
                        default="I am loading data.", required=False)
    parser.add_argument('-name', help='A pretty name to register this unit with.',
                        default="Data Loader", required=False)
    parser.add_argument('-q', help='Which quer[ies] to run. If multiple queries are given, write as CSV.',
                        default=False, required=False)
    ssb_args = parser.parse_args()

    client = tcp_client.TCPClient(
        unit_type=msg.UnitType.QueryPlaner, unit_info=ssb_args.info, name=ssb_args.name)

    """ Register custom callbacks """
    client.register_callback(
        msg.TCPPackageType.UuidForUnitResponse, update_uuids)

    """ Connect and wait until fully established """
    client.connect(ssb_args.ip, ssb_args.port)

    query_list = []
    if ssb_args.q:
        qs = ssb_args.q.split(",")
        query_list.extend([query.strip() for query in qs])
    
    execute_ssb(client, query_list)
