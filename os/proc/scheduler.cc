#include "scheduler.h"
#include <proc/process.h>

#include <arch/timer.h>
#include <utils/log.h>

task_queue kernel_task_queue;

process_scheduler kernel_process_scheduler[NCPU];


int stride_cmp(uint64 a, uint64 b)
{	
	
	if (a==b){
		return 0;
	}
	else if (a<b){
		if(b-a>BIG_STRIDE/2){ // a overflowed
			return 1;
		}else{
			return -1;
		}
	}else{
		if(a-b>BIG_STRIDE/2){ // b overflowed
			return -1;
		}else{
			return 1;
		}
	}
}

shared_ptr<process> task_queue::pop() {
    auto guard = make_lock_guard(lock);
    
    cpu* c = cpu::my_cpu();
    int core_id = c->get_core_id();

    uint64 min_stride = ~0ULL;

    if(queue.empty()){
        return nullptr;
    }

    auto min_it = queue.end();
    for(auto it = queue.begin(); it != queue.end(); ++it) {

        if(!(*it)->ready()){
            continue;
        }
        if((*it)->binding_core!=-1 && (*it)->binding_core!=core_id){
            continue;
        }
        if (stride_cmp((*it)->stride, min_stride) < 0) {
            min_stride = (*it)->stride;
            min_it = it;
        }
    }

    if(min_it == queue.end()){
        return nullptr;
    }

    auto ret = *min_it;
    queue.erase(min_it);

    return ret;
}

void task_queue::push(shared_ptr<process> proc) {
    auto guard = make_lock_guard(lock);
    queue.push_back(proc);
}

bool run_process(process* p) {

    bool reschedule = false;
    // save interrupt state
    uint64 sie = r_sie();
    uint64 sstatus = r_sstatus();
    uint64 stvec = r_stvec();


    reschedule = p->run();

    // restore interrupt state
    w_sie(sie);
    w_sstatus(sstatus);
    w_stvec(stvec);
    
    return reschedule;
}


void process_scheduler::run()
{
    uint64 busy = 0;
    uint64 all = 0;
    uint64 timestamp1 = r_cycle();
    while(true)
    {

        
        auto process = shared_queue->pop();

        if(!process) {
            warnf("no process to run");
            break;
        }

        debug_core("run process %s", process->get_name());

        process->schedule_lock.lock();

        uint64 busy_start = r_cycle();
        process->last_start_time = timer::get_time_ms();
        uint64 pass = BIG_STRIDE / (process->priority);
        process->stride += pass;

        process->schedule_lock.unlock();
        
        cpu::my_cpu()->set_process(process.get());
        bool reschedule = run_process(process.get());
        cpu::my_cpu()->set_process(nullptr);

        process->schedule_lock.lock();
        busy += r_cycle() - busy_start;
        uint64 time_delta = timer::get_time_ms() - process->last_start_time;
        process->cpu_time += time_delta;
        process->schedule_lock.unlock();

        if (reschedule) {
            shared_queue->push(process);
        }

        // sample cpu usage
        uint64 now = r_cycle();
        all += now - timestamp1;
        timestamp1 = now;
        // sample rate 10 Hz
        if (all > (timer::MS_TO_CYCLE(100)))
        {
            cpu *core = cpu::my_cpu();
            core->sample(all, busy);
            all = 0;
            busy = 0;
        }
    }
}